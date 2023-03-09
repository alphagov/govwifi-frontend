#!/bin/sh

if [ -z "$HEALTH_CHECK_RADIUS_KEY" ]; then
    export HEALTH_CHECK_RADIUS_KEY=testing123
fi

if [ -z "$AUTHORISATION_API_BASE_URL" ]; then
    export AUTHORISATION_API_BASE_URL=http://127.0.0.1
fi

if [ -z "$LOGGING_API_BASE_URL" ]; then
    export LOGGING_API_BASE_URL=http://127.0.0.1
fi

if [ -z "$HEALTH_CHECK_IDENTITY" ]; then
    export HEALTH_CHECK_IDENTITY=abcdef
fi

if [ -z "$HEALTH_CHECK_PASSWORD" ]; then
    export HEALTH_CHECK_PASSWORD=FoxCatBear
fi

if [ -z "$HEALTH_CHECK_SSID" ]; then
    export HEALTH_CHECK_SSID=govwifi
fi
