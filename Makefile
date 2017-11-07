LIBS=../mosquitto-ssl/mosquitto-1.4.2/lib/libmosquitto.so.1
mh: mh.o
	$(CC) $(LDFLAGS) $(LIBS) mh.o kmlgen.o xmlwriter.o -lpthread -o mh -I.

mh.o: mh.c
	$(CC) $(CFLAGS) -c mh.c kmlgen.c xmlwriter.c -I.

clean:
	rm *.o mh
