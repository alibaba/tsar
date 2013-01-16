#!/bin/sh

if test $# -ne 2; then
    echo "Usage: `basename $0` tsarfile pngfile" 1>&2
    exit 1
fi

datafile=$1.tmp

grep ":" $1 | grep -v '\-\-' | awk '{print $1 " " $2 " " $3 " " $4}' > $datafile

start=`head -n1 $datafile | awk '{print $1}'`
end=`tail -n1 $datafile | awk '{print $1}'`
duration="$start - $end"

gnuplot << EOF
set terminal png
set output "$2"
set title "load ($duration)"
set xlabel "time"
set xdata time
set timefmt "%d/%m-%H:%M"
set format x "%H:%M"
plot "$datafile" using 1:2 title "load1" with lines, "$datafile" using 1:3 title "load5" with lines, "$datafile" using 1:4 title "load15" with lines
EOF

rm $datafile
