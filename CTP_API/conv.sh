for file in *.h
do
    iconv -f gb2312 -t utf8 "$file">"$file.new" &&
    mv -f "$file.new" "$file"
done
