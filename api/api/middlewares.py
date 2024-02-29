# Copyright (C) 2015, Wazuh Inc.
# Created by Wazuh, Inc. <info@wazuh.com>.
# This program is a free software; you can redistribute it and/or modify it under the terms of GPLv2

import json
import hashlib
import time
import logging
import base64
import jwt

from starlette.requests import Request
from starlette.responses import Response
from starlette.middleware.base import BaseHTTPMiddleware, RequestResponseEndpoint

from connexion.lifecycle import ConnexionRequest
from connexion.security import AbstractSecurityHandler

from secure import Secure, ContentSecurityPolicy, XFrameOptions, Server

from wazuh.core.utils import get_utc_now

from api import configuration
from api.alogging import custom_logging
from api.authentication import generate_keypair, JWT_ALGORITHM
from api.api_exception import BlockedIPException, MaxRequestsException

# Default of the max event requests allowed per minute
MAX_REQUESTS_EVENTS_DEFAULT = 30

# Variable used to specify an unknown user
UNKNOWN_USER_STRING = "unknown_user"

# Run_as login endpoint path
RUN_AS_LOGIN_ENDPOINT = "/security/user/authenticate/run_as"
LOGIN_ENDPOINT = '/security/user/authenticate'

# API secure headers
server = Server().set("Wazuh")
csp = ContentSecurityPolicy().set('none')
xfo = XFrameOptions().deny()
secure_headers = Secure(server=server, csp=csp, xfo=xfo)

logger = logging.getLogger('wazuh-api')
start_stop_logger = logging.getLogger('start-stop-api')

ip_stats = dict()
ip_block = set()
general_request_counter = 0
general_current_time = None
events_request_counter = 0
events_current_time = None


async def access_log(request: ConnexionRequest, response: Response, prev_time: time):
    """Generate Log message from the request."""

    time_diff = time.time() - prev_time

    context = request.context if hasattr(request, 'context') else {}
    headers = request.headers if hasattr(request, 'headers') else {}
    path = request.scope.get('path', '') if hasattr(request, 'scope') else ''
    host = request.client.host if hasattr(request, 'client') else ''
    method = request.method if hasattr(request, 'method') else ''
    query = dict(request.query_params) if hasattr(request, 'query_params') else {}
    # If the request content is valid, the _json attribute is set when the
    # first time the json function is awaited. This check avoids raising an
    # exception when the request json content is invalid.
    body = await request.json() if hasattr(request, '_json') else {}

    if 'password' in query:
        query['password'] = '****'
    if 'password' in body:
        body['password'] = '****'
    if 'key' in body and '/agents' in path:
        body['key'] = '****'

    # Get the username from the request. If it is not found in the context, try
    # to get it from the headers using basic or bearer authentication methods.
    user = UNKNOWN_USER_STRING
    if headers and not (user := context.get('user', None)):
        auth_type, user_passw = AbstractSecurityHandler.get_auth_header_value(request)
        if auth_type == 'basic':
            user, _ = base64.b64decode(user_passw).decode("latin1").split(":", 1)
        elif auth_type == 'bearer':
            try:
                s = jwt.decode(user_passw, generate_keypair()[1],
                            algorithms=[JWT_ALGORITHM],
                            audience='Wazuh API REST',
                            options={'verify_exp': False})
                user = s['sub']
            except jwt.exceptions.PyJWTError:
                pass

    # Get or create authorization context hash
    hash_auth_context = context.get('token_info', {}).get('hash_auth_context', '')
    # Create hash if run_as login

    if not hash_auth_context and path == RUN_AS_LOGIN_ENDPOINT:
        hash_auth_context = hashlib.blake2b(json.dumps(body).encode(),
                                            digest_size=16).hexdigest()

    custom_logging(user, host, method, path, query, body, time_diff, response.status_code,
                   hash_auth_context=hash_auth_context, headers=headers)
    if response.status_code == 403 and \
        path in {LOGIN_ENDPOINT, RUN_AS_LOGIN_ENDPOINT} and \
            method in {'GET', 'POST'}:
        logger.warning(f'IP blocked due to exceeded number of logins attempts: {host}')


def check_blocked_ip(request: Request):
    """Blocks/unblocks the IPs that are requesting an API token.

    Parameters
    ----------
    request : Request
        HTTP request.
    block_time : int
        Block time used to decide if the IP is going to be unlocked.

    """
    global ip_block, ip_stats
    access_conf = configuration.api_conf['access']
    block_time = access_conf['block_time']
    try:
        if get_utc_now().timestamp() - block_time >= ip_stats[request.client.host]['timestamp']:
            del ip_stats[request.client.host]
            ip_block.remove(request.client.host)
    except (KeyError, ValueError):
        pass
    if request.client.host in ip_block:
        raise BlockedIPException(
            status=403,
            title="Permission Denied",
            detail="Limit of login attempts reached. The current IP has been blocked due "
                    "to a high number of login attempts")


def check_rate_limit(
    request_counter_key: str,
    current_time_key: str,
    max_requests: int,
    error_code: int
) -> int:
    """Check that the maximum number of requests per minute
    passed in `max_requests` is not exceeded.

    Parameters
    ----------
    request_counter_key : str
        Key of the request counter variable to get from globals() dict.
    current_time_key : str
        Key of the current time variable to get from globals() dict.
    max_requests : int
        Maximum number of requests per minute permitted.
    error_code : int
        error code to return if the counter is greater than max_requests.

    Return
    ------
        0 if the counter is greater than max_requests
        else error_code.
    """
    if not globals()[current_time_key]:
        globals()[current_time_key] = get_utc_now().timestamp()

    if get_utc_now().timestamp() - 60 <= globals()[current_time_key]:
        globals()[request_counter_key] += 1
    else:
        globals()[request_counter_key] = 0
        globals()[current_time_key] = get_utc_now().timestamp()

    if globals()[request_counter_key] > max_requests:
        return error_code

    return 0


class CheckRateLimitsMiddleware(BaseHTTPMiddleware):
    """Rate Limits Middleware."""

    async def dispatch(self, request: Request, call_next: RequestResponseEndpoint) -> Response:
        """"Check request limits per minute."""
        max_request_per_minute = configuration.api_conf['access']['max_request_per_minute']
        error_code = check_rate_limit(
            'general_request_counter',
            'general_current_time',
            max_request_per_minute,
            6001)

        if request.url.path == '/events':
            error_code = check_rate_limit(
                'events_request_counter',
                'events_current_time',
                MAX_REQUESTS_EVENTS_DEFAULT,
                6005)

        if error_code:
            raise MaxRequestsException(code=error_code)
        else:
            return await call_next(request)


class CheckBlockedIP(BaseHTTPMiddleware):
    """Rate Limits Middleware."""

    async def dispatch(self, request: Request, call_next: RequestResponseEndpoint) -> Response:
        """"Update and check if the client IP is locked."""
        if request.url.path in {LOGIN_ENDPOINT, RUN_AS_LOGIN_ENDPOINT} \
           and request.method in {'GET', 'POST'}:
            check_blocked_ip(request)
        return await call_next(request)


class WazuhAccessLoggerMiddleware(BaseHTTPMiddleware):
    """Middleware to log custom Access messages."""

    async def dispatch(self, request: Request, call_next: RequestResponseEndpoint) -> Response:
        """Log Wazuh access information.

        Parameters
        ----------
        request : Request
            HTTP Request received.
        call_next :  RequestResponseEndpoint
            Endpoint callable to be executed.

        Returns
        -------
        Response
            Returned response.
        """
        prev_time = time.time()
        response = await call_next(request)
        await access_log(ConnexionRequest.from_starlette_request(request), response, prev_time)
        return response


class SecureHeadersMiddleware(BaseHTTPMiddleware):
    """Secure headers Middleware."""

    async def dispatch(self, request: Request, call_next: RequestResponseEndpoint) -> Response:
        """Check and modifies the response headers with secure package.

        Parameters
        ----------
        request : Request
            HTTP Request received.
        call_next :  RequestResponseEndpoint
            Endpoint callable to be executed.

        Returns
        -------
        Response
            Returned response.
        """
        resp = await call_next(request)
        secure_headers.framework.starlette(resp)
        return resp
