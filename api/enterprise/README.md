# Overview
This document describes how to use the Parsec Enterprise API to get a `sessionID` associated with a provided `external_id`.

Please see the cURL command example below. Please reach out to your Parsec Enterprise contact if you have any questions.

# Important Concepts

### API Key
The Enterprise API Key given to you by your Parsec Enterprise contact must be kept private and only used by your server backend, **NOT** included with your client applications. This API Key is used for calls to `/enterprise/sessions` and can be used to create a `sessionID` for any `external_id` on your system.

### Session ID
A `sessionID` is string token that is required for secure use of the Parsec API. It is sensitive information, similar to a password, so your application must store the `sessionID` securely. The Parsec SDK needs a `sessionID` to host via `ParsecHostStart` and connect via `ParsecClientConnect`.

### External ID
An `external_id` is a unique value indentifying a user to your orginzation. This will most likely be your organization's concept of a User ID.

# API

## Sessions - /enterprise/sessions
The `/enterprise/sessions` endpoint allows you to obtain a `sessionID` associated with a unique `external_id` from your system (typically, a user ID).

This endpoint may be called multiple times for the same `external_id`, it will return a unique `sessionID` each time for a new device. You do not need to request a new `sessionID` every time a device connects -- rather, the intent is that each unique device should have one `sessionID`.

* Request
  ```text
  POST https://kessel-api.parsecgaming.com/enterprise/

  Headers:
    x-api-key: YOUR_ENTERPRISE_API_KEY
    Content-Type: application/json

  Body:
    {
        "external_id": "SOME_UNIQUE_ID"
    }
  ```

* Response
  ```javascript
  STATUS 201

  {
    "session": {
      "id": "9066e9f2ac56648e83d68c7b5902236361e98f725413af6f8fac0dab720cd270", // The sessionID
      "expires_at" : {UNIX Epoch time integer},
      "host_peer_id": "1QA0qE1h0aBW7GKRTXCU0ZInLVZ",
      "client_peer_id": "1QA0qBt04fWhJqgII3a1TnsG8Ls",
    }
  }
  ```

* cURL Example
  ```bash
  curl -X "POST" "https://kessel-api.parsecgaming.com/enterprise/sessions" \
    -H 'x-api-key: YOUR_API_KEY' \
    -H 'Content-Type: application/json' \
    -d $'{ "external_id": "SOME_UNIQUE_ID" }'
  ```

# Fetching Available Hosts

See [/api/personal](/api/personal#hosts---v2hosts) for documentation of the `/v2/hosts` API endpoint. `/v2/hosts` is the only way to retrieve available hosts.

The `external_id` you provided during `/enterprise/sessions` will be made available in the `user.name` property of the `/v2/hosts` response. This may be useful for looking up additional user metadata associated with the guest.
