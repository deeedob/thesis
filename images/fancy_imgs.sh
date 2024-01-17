#!/bin/bash

images_dir=$(pwd)
clean_dir="$images_dir/clean"
this="fancy_imgs.sh"

for img in "$clean_dir"/*.png; do
    filename=$(basename "$img")
    magick "$img" \( +clone -alpha extract -virtual-pixel black -spread 10 -blur 0x3 -threshold 50% -spread 1 -blur 0x.7 \) -alpha off -compose Copy_Opacity -composite "$images_dir/$filename"
    magick -page +8+8 "$filename" -alpha set \( +clone -background navy -shadow 60x8+8+8 \) +swap -background none -mosaic "$images_dir/$filename"
done

echo "Image processing complete. Backup located at $backup_dir"

