from typing import Optional

from cryptography.hazmat.primitives.asymmetric import ec
from fastapi import Request, status
from fastapi.exceptions import HTTPException
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from jwt import decode, encode
from jwt.exceptions import PyJWTError

from api.authentication import generate_keypair, JWT_ISSUER, EXPIRED_TOKEN, INVALID_TOKEN
from comms_api.routers.exceptions import HTTPError
from wazuh.core.utils import get_utc_now

JWT_AUDIENCE = 'Wazuh Agent Comms API'
JWT_ALGORITHM = 'ES256'
JWT_EXPIRATION = 900


class JWTBearer(HTTPBearer):
    def __init__(self):
        super(JWTBearer, self).__init__(auto_error=True)

    async def __call__(self, request: Request) -> str:
        """Get JWT token from the request header and validate it.

        Parameters
        ----------
        request : Request
            HTTP request.
        
        Raises
        ------
        HTTPError
            Invalid token error.

        Returns
        -------
        str
            HTTP Authorization header credentials.        
        """
        try:
            credentials: Optional[HTTPAuthorizationCredentials] = await super(JWTBearer, self).__call__(request)
            payload = decode_token(credentials.credentials)
            # Store the agent UUID in the request context
            request.state.agent_uuid = payload.get('uuid', '')
        except (HTTPException, Exception) as exc:
            raise HTTPError(message=str(exc), status_code=status.HTTP_403_FORBIDDEN)

        return credentials.credentials


def decode_token(token: str) -> dict:
    """Decode a JWT formatted token.

    Parameters
    ----------
    token : str
        Encoded JWT token.

    Raises
    ------
    Exception
        If the token validation fails.

    Returns
    -------
    dict
        Dictionary with the token payload.
    """
    try:
        _, public_key = generate_keypair(ec.SECP256R1())
        payload = decode(token, public_key, algorithms=[JWT_ALGORITHM], audience=JWT_AUDIENCE)

        if (payload['exp'] - payload['iat']) != JWT_EXPIRATION:
            raise Exception(INVALID_TOKEN)

        current_timestamp = int(get_utc_now().timestamp())
        if payload['exp'] <= current_timestamp:
            raise Exception(EXPIRED_TOKEN)

        return payload
    except PyJWTError as exc:
        raise Exception(INVALID_TOKEN) from exc


def generate_token(uuid: str) -> str:
    """Generate an encoded JWT token.

    Parameters
    ----------
    uuid : str
        Unique agent identifier.

    Returns
    -------
    str
        Encoded JWT token.
    """
    timestamp = int(get_utc_now().timestamp())
    payload = {
        'iss': JWT_ISSUER,
        'aud': JWT_AUDIENCE,
        'iat': timestamp,
        'exp': timestamp + JWT_EXPIRATION,
        'uuid': uuid,
    }
    private_key, _ = generate_keypair(ec.SECP256R1())

    return encode(payload, private_key, algorithm=JWT_ALGORITHM)
