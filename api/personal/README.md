# Overview
This document describes the Parsec API endpoints that allow a user to authenticate, obtain a `sessionID`, and list available hosts.

Please see the cURL command examples below and the example scripts in Python. Please reach out at `sdk@parsecgaming.com` if you have any questions.

The Python examples require Python 3.0 and the `requests` package:

`pip3 install requests`

# Important Concepts

### Session ID
A `sessionID` is string token that is required for secure use of the Parsec API. It is sensitive information, similar to a password, so your application must store the `sessionID` securely. The Parsec SDK needs a `sessionID` to host via `ParsecHostStart` and connect via `ParsecClientConnect`.

### Hosts
A "Host" refers to a computer or game that other users can connect to via Parsec.

# API

## Auth - /v1/auth
The `/v1/auth` call may return a `403` if two factor auth is required. If using two factor authentication, resubmit the request with the `tfa` property set in the body to your TFA code.

* Request
  ```text
  POST https://api.parsecgaming.com/v1/auth/

  Headers:
    Content-Type: application/json

  Body:
    {
      "email": "YOUR_EMAIL_ADDRESS",
      "password": "YOUR_PASSWORD",
      "tfa": "TFA_CODE"
    }
  ```

* Response
  ```javascript
  STATUS 200

  {
    "data": {
      "id": "9066e9f2ac56648e83d68c7b5902236361e98f725413af6f8fac0dab720cd270", // The sessionID
    }
  }
  ```

* cURL Example
  ```bash
  curl -X "POST" "https://api.parsecgaming.com/v1/auth" \
    -H 'Content-Type: application/json' \
    -d $'{ "email": "YOUR_EMAIL_ADDRESS", "password": "YOUR_PASSWORD" }'
  ```

## Hosts - /v2/hosts
The `/v2/hosts` endpoint requires a `sessionID`, and will return all the online hosts that the given `sessionID` may connect to.

The `/v2/hosts` endpoint can return hosts in desktop mode or game mode by specifying the `mode` query string parameter. It may also return private game mode hosts if `?mode=game&public=false` is provided.

* Request
    ```text
    GET https://kessel-api.parsecgaming.com/v2/hosts/

    Query String:
      mode=desktop|game
      public=true|false

    Headers:
      Authorization: Bearer YOUR_SESSION_ID
    ```

* Response
    ```javascript
    STATUS 200

    {
      "data": [
        {
          "peer_id": "1QA0qE1h0aBW7GKRTXCU0ZInLVZ",
          "user":{
            "id":1337,
            "name":"The Dude",
            "warp":false
          },
          "game_id":"",
          "build":"150-32",
          "description":"",
          "max_players":20,
          "mode":"desktop",
          "name":"LEBOWSKI",
          "players":0,
          "public":false,
          "self":false // Indicates if the host that made the /v2/hosts call is attached to the same sessionID
        }
      ],
      "has_more": false
  }
  ```

* cURL Example
  ```bash
  curl -X "GET" "https://kessel-api.parsecgaming.com/v2/hosts?mode=desktop&public=false" \
    -H 'Authorization: Bearer YOUR_SESSION_ID'
  ```

To host's `peer_id` above is used to make a connection via `ParsecClientConnect` in the Parsec SDK.
