#!/bin/sh

if test $# -ne 2; then
    echo "Usage: `basename $0` tsarfile pngfile" 1>&2
    exit 1
fi

datafile=$1.tmp

grep ":" $1 | awk 'function f(a){if(index(a,"G")>0) return a*1000; else return a} {printf "%s %.2f %.2f %.2f %.2f %.2f\n", $1, f($2),f($3),f($4),f($5),f($6)}' > $datafile

start=`head -n1 $datafile | awk '{print $1}'`
end=`tail -n1 $datafile | awk '{print $1}'`
duration="$start - $end"
total=`head -n1 $datafile | awk '{print $6}'`

gnuplot << EOF
set terminal png
set output "$2"
set title "memory utilization, total = ${total}M ($duration)"
set xlabel "time"
set ylabel "M bytes"
set yrange [ 0 : $total * 1.1 ]
set xdata time
set timefmt "%d/%m-%H:%M"
set format x "%H:%M"
plot "$datafile" using 1:2 title "free" with lines, "$datafile" using 1:3 title "used" with lines, "$datafile" using 1:4 title "buff" with lines, "$datafile" using 1:5 title "cache" with lines

EOF

rm $datafile
