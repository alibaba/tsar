#!/bin/sh

if test $# -ne 2; then
    echo "Usage: `basename $0` tsarfile pngfile" 1>&2
    exit 1
fi

datafile=$1.tmp

grep ":" $1 | awk 'function f(a) { if (index(a, "G") > 0) return a * 1000 * 1000; else if (index(a, "M") > 0) return a * 1000; else return a} {printf "%s %.2f %.2f\n", $1, f($2), f($3)}' > $datafile

start=`head -n1 $datafile | awk '{print $1}'`
end=`tail -n1 $datafile | awk '{print $1}'`
duration="$start - $end"

gnuplot << EOF
set terminal png
set output "$2"
set title "bytes in/out ($duration)"
set xlabel "time"
set ylabel "K bytes"
set xdata time
set timefmt "%d/%m-%H:%M"
set format x "%H:%M"
plot "$datafile" using 1:2 title "bytes in" with lines, "$datafile" using 1:3 title "bytes out" with lines

EOF

rm $datafile
