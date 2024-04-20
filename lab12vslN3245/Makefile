# Определяем компилятор и флаги 
CC=gcc
CFLAGS=-Wall -Wextra -Werror -O3

# Название файла
TARGET=lab11vslN3245

# Цель 
all: clean $(TARGET)

$(TARGET): lab11vslN3245.c
	$(CC) $(CFLAGS) lab11vslN3245.c -o $(TARGET)

# Цель для очистки проекта 
clean:
	rm -f $(TARGET)