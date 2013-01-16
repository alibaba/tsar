#!/bin/sh

if test $# -ne 2; then
    echo "Usage: `basename $0` tsarfile pngfile" 1>&2
    exit 1
fi

datafile=$1.tmp

grep ":" $1 | grep -v '\-\-' | awk '{print $1 " " $6}' > $datafile

start=`head -n1 $datafile | awk '{print $1}'`
end=`tail -n1 $datafile | awk '{print $1}'`
duration="$start - $end"

gnuplot << EOF
set terminal png
set output "$2"
set title "TCP retransmission rate ($duration)"
set xlabel "time"
set ylabel "percent (%)"
set yrange [ 0 : 10 ]
set xdata time
set timefmt "%d/%m-%H:%M"
set format x "%H:%M"
plot "$datafile" using 1:2 title "rate" with lines
EOF

rm $datafile
