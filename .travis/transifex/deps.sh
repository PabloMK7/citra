#!/bin/bash -ex

sudo pip install transifex-client
echo $'[https://www.transifex.com]\nhostname = https://www.transifex.com\nusername = api\npassword = '"$TRANSIFEX_API_TOKEN"$'\n' > ~/.transifexrc
