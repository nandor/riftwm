#!/bin/bash
glxgears > /dev/null &
exec build/riftwm
