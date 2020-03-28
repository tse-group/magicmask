# see https://stackoverflow.com/questions/12306223/how-to-manually-create-icns-files-using-iconutil

TARGET=icon_v01

all: build

build: $(TARGET).svg
	inkscape --export-png=$(TARGET)_16x16.png --export-area-page --export-width=16  --export-height=16 $(TARGET).svg
	inkscape --export-png=$(TARGET)_32x32.png --export-area-page --export-width=32  --export-height=32 $(TARGET).svg
	inkscape --export-png=$(TARGET)_64x64.png --export-area-page --export-width=64  --export-height=64 $(TARGET).svg
	inkscape --export-png=$(TARGET)_128x128.png --export-area-page --export-width=128  --export-height=128 $(TARGET).svg
	inkscape --export-png=$(TARGET)_256x256.png --export-area-page --export-width=256  --export-height=256 $(TARGET).svg
	inkscape --export-png=$(TARGET)_512x512.png --export-area-page --export-width=512  --export-height=512 $(TARGET).svg
	inkscape --export-png=$(TARGET)_1024x1024.png --export-area-page --export-width=1024  --export-height=1024 $(TARGET).svg
	png2icns $(TARGET).icns $(TARGET)_512x512.png
	