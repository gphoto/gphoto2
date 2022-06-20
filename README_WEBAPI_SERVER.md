# GPhoto2 WEBAPI Server

I need to control my camera via a small embedded controller so i decided to implement this little server direct into the wonderful gphoto2 application. 

2022 June, Thorsten Ludewig (t.ludewig@gmail.com)

## clone with submodule (mongoose webserver) and build

  ```plain
  git clone --recurse-submodules <repository path>
  cd gphoto2/gphoto2/mongoose
  git checkout tags/7.7
  cd ../..
  ``` 

  See README.md for further build info.

## Run server

`gphoto2-webapi --server` default port 8866

binding to a different port 

`gphoto2-webapi --server-url=http://0.0.0.0:9999 --server`

binding to a specific ip and or port

`gphoto2-webapi --server-url=http://127.0.0.1:8866 --server`

## API Calls - all HTTP GET

Nearly all responses are [JSON](https://json.org) formated.

### general

- `http://<server ip>:8866/` 

  version information for now

- `http://<server ip>:8866/api/version` 

  version information

- `http://<server ip>:8866/api/auto-detect` 

  show detected camera info
  ```jsonc
  {
    result: [
              {
                model: "Canon EOS 550D",
                port: "usb:003,005"
              }
            ],
    return_code: 0
  }
  ```

- `http://<server ip>:8866/api/trigger-capture` 

  trigger capture image

- `http://<server ip>:8866/api/capture-image`

  response

  ```jsonc
  {
    images: [
      {
        info: {
          name: "IMG_0322.CR2",
          folder: "/store_00020001/DCIM/100CANON",
          path: "/store_00020001/DCIM/100CANON/IMG_0322.CR2",
          mtime: 1655724028,
          size: 21937775,
          type: "image/x-canon-cr2"
        },
        download: false
      },
      {
        info: {
          name: "IMG_0322.JPG",
          folder: "/store_00020001/DCIM/100CANON",
          path: "/store_00020001/DCIM/100CANON/IMG_0322.JPG",
          mtime: 1655724026,
          size: 4038986,
          height: 3456,
          width: 5184,
          type: "image/jpeg"
        },
        download: false
      }
    ],
    return_code: 0
  }
  ```

- `http://<server ip>:8866/api/capture-image-download` 

  response
  
  ```jsonc
  {
    images: [
      {
        info: {
          name: "IMG_0321.CR2",
          folder: "/store_00020001/DCIM/100CANON",
          path: "/store_00020001/DCIM/100CANON/IMG_0321.CR2",
          mtime: 1655723756,
          size: 21888193,
          type: "image/x-canon-cr2"
        },
        download: true,
        local_folder: "/home/th/Projects/gphoto2",
        keeping_file_on_camera: true
      },
      {
        info: {
          name: "IMG_0321.JPG",
          folder: "/store_00020001/DCIM/100CANON",
          path: "/store_00020001/DCIM/100CANON/IMG_0321.JPG",
          mtime: 1655723756,
          size: 4113431,
          height: 3456,
          width: 5184,
          type: "image/jpeg"
        },
        download: true,
        local_folder: "/home/th/Projects/gphoto2",
        keeping_file_on_camera: true
      }
    ],
    return_code: 0
  }
  ```


### server

- `http://<server ip>:8866/api/server/shutdown` 

  shutdown http server

### config

- `http://<server ip>:8866/api/config/list` 

  list available config names

- `http://<server ip>:8866/api/config/list-all` 

  list config including current and available config values

- `http://<server ip>:8866/api/config/set-index/<config name>?i=<index value>` 

  set the `config name` to the specified `index value` 

- `http://<server ip>:8866/api/config/get/<config name>` 

  get single config property

  sample request

  `http://localhost:8866/api/config/get/main/imgsettings/colorspace`

  response

  ```jsonc
  {
    path: "/main/imgsettings/colorspace",
    label: "Farbraum",
    readonly: false,
    type: "MENU",
    current: "sRGB",
    choice: [
        {
          index: 0,
          value: "sRGB"
        },
        {
          index: 1,
          value: "AdobeRGB"
        }
      ],
    return_code: 0
  }
  ```


### file

- `http://<server ip>:8866/api/file/get/<path to file/image>` 

  response - native file

- `http://<server ip>:8866/api/file/exif/<path to image>` 

  response 
  
  ```jsonc
  {
    make: "Canon",
    model: "Canon EOS 550D",
    orientation: "Oben links",
    xresolution: "72",
    yresolution: "72",
  ...
  }
  ``` 

- `http://<server ip>:8866/api/file/list/<path to folder>` 

  response
  
  ```jsonc
  {
    path: "/store_00020001/DCIM/",
    files: [
             {
               name: "100CANON",
               isFolder: true
             }
           ],
    entries: 1,
    return_code: 0
  }
  ```

- `http://<server ip>:8866/api/file/delete/<path to file>` 

  remove specified file

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
