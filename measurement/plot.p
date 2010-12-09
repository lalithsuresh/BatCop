set te post enhanced landscape color
set output "wifi-runtime.ps"


plot "./5mins-idle-connected.log" using ($0*30):1 with linespoints lt 3, \
"./5mins-idle-connected2.log" using ($0*30):1 with linespoints lt 3, \
"./5mins-idle-radio-on.log" using ($0*30):1 with linespoints lt 2, \
"./5mins-idle-radio-on2.log" using ($0*30):1 with linespoints lt 2, \
"./5mins-idle-radio-off.log" using ($0*30):1 with linespoints lt 1, \
"./5mins-idle-radio-off2.log" using ($0*30):1 with linespoints lt 1
