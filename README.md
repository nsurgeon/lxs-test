# Test project
Task given as an prerequisite for technical interview

## Task definition

Original:

    Написать программу на C++ 11 (или C++ 14) которая выполняет избыточные вычисления используя несколько потоков: 
    o Потоки А-1..А-M генерируют блоки случайных данных. Количество потоков, блоков и размер блока задается параметрами командной строки. Количество блоков может быть очень большим. Блок генерируется одним потоком. Разные блоки могут быть сгенерированы разными потоками.
    o Потоки Б-1..Б-N вычисляют CRC32 (можно использовать готовую реализацию) для блоков сгенерированных потоками A. Количество потоков Б задается параметром командной строки. Для каждого блока каждый поток Б вычислит своё значение CRC32.
    o Когда все потоки Б вычислят CRC32 для какого-то блока, надо сравнить полученные значения и если они не совпадают записать блок в файл и вывести сообщение в std::cout.
    o Потоки A и Б должны работать параллельно.



## Solution


### Dependencies

- boost library: 
    - command line arguments parsing
    - filesystem
    - system

#### Build boost

    ./bootstrap.sh --prefix=<install path prefix>
    ./b2 --prefix=<install path> --build-dir=<build path> link=shared variant=release threading=multi    

### Project structure

* includes - files with headers
* stc      - application sources
* main.cpp - entry point

# TODO
- logger
- unit tests