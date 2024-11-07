![eklipsed](/eklipsed.jpg)
<p align="center">eklipsed utilities -*ekutils*-</p>

> A collection of simple, opt-in utilities

## What is ekutils?
I find that I implement the same things time and time again for a projects, so
I wanted to create a **simple**, or atleast relativly simple library that I
could just drop the files into any project and use the things that I need.

## How to use ekutils?
### How to build:
Just drop the files into your project and edit ek.h to define what features
you want to have. Read ek.h to see what features depend on other features in
ekutils.

### How to test:
Just run
```
make && ./build/test
```
To test the utility library

## What features will be in ekutils?
- [x] string views
- [x] dynamic string buffers
- [x] generic allocation interface
- [x] pool allocater
- [x] arena allocater
- [x] deadass simple logging library
- [x] vectors
- [x] robin-hood hash maps
- [x] string hash function
- [x] simple testing framework

## License
[GLWTSPL](/LICENSE)


