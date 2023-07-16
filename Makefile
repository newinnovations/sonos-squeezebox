FLAGS_SL = -g -O3 -Wall -fno-common -Isqueezelite

OBJS = sonos-squeezebox.o sbstreamer.o sbencoder.o sonos-status.o

OBJS_SL = squeezelite.o \
	output_sonos.o \
	squeezelite/slimproto.o \
	squeezelite/decode.o \
	squeezelite/buffer.o \
	squeezelite/stream.o \
	squeezelite/utils.o \
	squeezelite/output.o \
	squeezelite/output_pack.o \
	squeezelite/flac.o \
	squeezelite/pcm.o \
	squeezelite/vorbis.o \
	squeezelite/faad.o \
	squeezelite/mad.o \
	squeezelite/mpg.o

all: sonos-squeezebox

noson/noson/libnoson.a:
	cmake -D CMAKE_BUILD_TYPE=Release -S noson -B noson
	make -C noson

%.o: %.cpp noson/noson/libnoson.a
	g++ -g -O3 -Wall -Inoson/noson/src -Inoson/noson/public/noson -c -o $@ $<

%.o: %.c
	gcc $(FLAGS_SL) -c -o $@ $<

squeezelite.o: squeezelite.cpp
	g++ $(FLAGS_SL) -c -o $@ $<

sonos-squeezebox: $(OBJS) $(OBJS_SL) noson/noson/libnoson.a
	g++ -g -o $@ $^ \
		-Lnoson/noson -lnoson \
		-lFLAC++ -lFLAC -lcrypto -lssl -lz \
		-lpthread -lm -lrt -ldl -lasound

clean:
	rm -f *.o squeezelite/*.o sonos-squeezebox
