gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"server.d" -MT"server.d" -o"server.o" "../server.c"
gcc  -o"HTTPServer"  ./base64.o ./server.o ./time.o  ./bstring/bstrlib.o   