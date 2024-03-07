#!/bin/bash

convert\
    -resize x500 ls.jpg\
    -resize x500 rungholt.jpg\
    -resize x500 kingsnake.jpg\
     +append collage-quicktests.jpg
