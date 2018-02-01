#!/bin/bash
# create multiresolution windows icon
ICON_DST=../../src/qt/res/icons/axe.ico

convert ../../src/qt/res/icons/axe-16.png ../../src/qt/res/icons/axe-32.png ../../src/qt/res/icons/axe-48.png ${ICON_DST}
