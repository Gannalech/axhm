include .env

LDFLAGS=${MY_MOSQUITTO_LIB}/libmosquitto.so.1

SRC=./src
BUILD=.

${BUILD}/mh: mh.o
	$(CC)  $(LIBS) ${BUILD}/mh.o ${BUILD}/kmlgen.o ${BUILD}/xmlwriter.o -lpthread -o mh -I${SRC} ${LIBS} $(LDFLAGS)

${BUILD}/mh.o: ${SRC}/mh.c
	$(CC) $(CFLAGS) -c ${SRC}/mh.c ${SRC}/kmlgen.c ${SRC}/xmlwriter.c -I${SRC}

clean:
	rm ${BUILD}/*.o ${BUILD}/mh
