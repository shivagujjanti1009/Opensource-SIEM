import json
from unittest.mock import MagicMock

import pytest
from fastapi import status
from fastapi.exceptions import RequestValidationError

from comms_api.routers.exceptions import HTTPError, http_error_handler, validation_exception_handler


@pytest.mark.asyncio
@pytest.mark.parametrize('message,code,status_code', [
    ('value', 1001, status.HTTP_400_BAD_REQUEST),
    ('value1', status.HTTP_408_REQUEST_TIMEOUT, status.HTTP_408_REQUEST_TIMEOUT),
    ('test', None, status.HTTP_500_INTERNAL_SERVER_ERROR)
])
async def test_http_error_handler(message, code, status_code):
    """Verify that the HTTP error handler works as expected."""
    mock_req = MagicMock()
    result = await http_error_handler(mock_req, HTTPError(message, code, status_code))
    body = json.loads(result.body)
    if code is None:
        code = status_code

    assert result.status_code == status_code
    assert body['message'] == message
    assert body['code'] == code


@pytest.mark.asyncio
@pytest.mark.parametrize('loc,msg,expected_msg', [
    (['agent', 'uuid'], 'value is not a valid string', 'agent.uuid value is not a valid string'),
    (['event', 'id'], 'value is not a valid integer', 'event.id value is not a valid integer')
])
async def test_validation_exception_handler(loc, msg, expected_msg):
    """Verify that the request validation exception handler works as expected."""
    mock_req = MagicMock()
    mock_err = {'loc': loc, 'msg': msg}
    result = await validation_exception_handler(mock_req, RequestValidationError([mock_err]))
    body = json.loads(result.body)

    assert result.status_code == status.HTTP_400_BAD_REQUEST
    assert body['message'] == expected_msg
    assert body['code'] == status.HTTP_400_BAD_REQUEST
