#!/bin/sh
# healthy once the server has written its status (the engine takes a while to load the game data)
curl -fsS "http://127.0.0.1:${LW_STATUS_PORT:-8080}/status.json" | grep -q uptimeSeconds
