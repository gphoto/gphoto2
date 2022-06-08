# GPhoto2 REST HTTP Server

I need to control my camera via a small embedded controller so i decided to implement this little server direct into the wonderful gphoto2 application. Most of the code is copied from `actions.c` only little modified for the Mongoose HTTP server output, so it was no witchcraft ;-)


2022 June, Thorsten Ludewig (t.ludewig@gmail.com)

## Run server

`gphoto2 --server` 

At this time the server port is fixed to port number: `8866`

## API Calls - all HTTP GET

All responses are [JSON](https://json.org) formated.

- `http://<server ip>:8866/` 

   version information for now

- `http://<server ip>:8866/api/server/shutdown` 

  shutdown http server

- `http://<server ip>:8866/api/version` 

  version information

- `http://<server ip>:8866/api/auto-detect` 

  show detected camera info

- `http://<server ip>:8866/api/trigger-capture` 

  trigger capture image

- `http://<server ip>:8866/api/config/list` 

  list available config names

- `http://<server ip>:8866/api/config/list/all` 

  list config including current and available config values

- `http://<server ip>:8866/api/config/set-index/<config name>?i=<index value>` 

  set the `config name` to the specified `index value` 

## Mongoose HTTP Server - LICENSE

```
Copyright (c) 2004-2013 Sergey Lyubka
Copyright (c) 2013-2021 Cesanta Software Limited
All rights reserved

This software is dual-licensed: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation. For the terms of this
license, see <http://www.gnu.org/licenses/>.

You are free to use this software under the terms of the GNU General
Public License, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

Alternatively, you can license this software under a commercial
license, as set out in <https://mongoose.ws/licensing/>.
```
