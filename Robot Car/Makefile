TARGET=hw7bryan

SOURCES=import_registers.c \
        enable_pwm_clock.c \
	wait_key.c \
        myCar.c

OBJECTS=$(patsubst %.c,%.o,$(SOURCES))

all: $(OBJECTS)
	gcc $(OBJECTS) -lpthread -lpigpio -lrt -lm -o $(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET)

%.o:%.c
	gcc -c $< -o $@