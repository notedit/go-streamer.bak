package main

import (
    "fmt"
    "unsafe"
)

/*

#include <stdlib.h>
#include "streamer.h"

#cgo LDFLAGS: -lavformat -lavdevice -lavcodec -lavutil
#cgo CFLAGS: -std=c11
#cgo pkg-config: libavcodec
*/
import "C"


type Input struct {
    vsInput     *C.struct_SInput
}



func main() {

    C.s_setup()

    inputFormat := "mp4"
    inputURL := "guigu.mp4"

    inputFormatC := C.CString(inputFormat)
    inputURLC := C.CString(inputURL)

    input := C.s_open_input(inputFormatC, inputURLC, C.bool(true))

    if input == nil {
        fmt.Printf("Unable to open input")
        C.free(unsafe.Pointer(inputFormatC))
        C.free(unsafe.Pointer(inputURLC))
    }

    inputi := &Input{
        vsInput: input,
    }

    fmt.Printf("%v", inputi)
    fmt.Printf("%v", input)

    select {}
}
