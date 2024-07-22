TARGET="../dist/usr/share/icons/hicolor"

for res in "32" "48" "64" "128"; do
  mkdir -p $TARGET/${res}x${res}/apps/ 
  cp $res.png $TARGET/${res}x${res}/apps/com.github.jsol.notes-editor.png
done
