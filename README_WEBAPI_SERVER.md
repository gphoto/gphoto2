# GPhoto2 WEBAPI Server

I need to control my camera via a small embedded controller so i decided to implement this little server direct into the wonderful gphoto2 application. 

2022 June, Thorsten Ludewig (t.ludewig@gmail.com)

## clone with submodule (mongoose webserver)

`git clone --recurse-submodules <repository path>` 

## Run server

`gphoto2-webapi --server` default port 8866

binding to a different port 

`gphoto2-webapi --server-url=http://0.0.0.0:9999 --server`

binding to a specific ip and or port

`gphoto2-webapi --server-url=http://127.0.0.1:8866 --server`

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

- `http://<server ip>:8866/api/capture-image`

response

```jsonc
{
  image_info: {
    name: "IMG_0264.JPG",
    folder: "/store_00020001/DCIM/100CANON",
    mtime: 1655542624,
    size: 4838064,
    height: 3456,
    width: 5184,
    type: "image/jpeg"
  },
  download: false,
  return_code: 0
}
```

- `http://<server ip>:8866/api/capture-image-download` 

response

```jsonc
{
  image_info: {
    name: "IMG_0265.JPG",
    folder: "/store_00020001/DCIM/100CANON",
    mtime: 1655542624,
    size: 4838064,
    height: 3456,
    width: 5184,
    type: "image/jpeg"
  },
  download: true,
  local_folder: "/home/user/Projects/gphoto2",
  keeping_file_on_camera: true,
  return_code: 0
}
```

- `http://<server ip>:8866/api/get-file/<path to file/image>` 

response 
- native file

- `http://<server ip>:8866/api/get-exif/<path to image>` 


## Mongoose HTTP Server - LICENSE

https://github.com/cesanta/mongoose


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
