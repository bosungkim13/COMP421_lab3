#!/bin/bash

make clean
make

/clear/courses/comp421/pub/bin/mkyfs
/clear/courses/comp421/pub/bin/yalnix -ly 10 yfs tests/cannonTest1

/clear/courses/comp421/pub/bin/mkyfs
/clear/courses/comp421/pub/bin/yalnix -ly 10 yfs tests/cannonTest2

/clear/courses/comp421/pub/bin/mkyfs
/clear/courses/comp421/pub/bin/yalnix -ly 10 yfs tests/cannonTest4

/clear/courses/comp421/pub/bin/mkyfs
/clear/courses/comp421/pub/bin/yalnix -ly 10 yfs tests/cannonTest5

/clear/courses/comp421/pub/bin/mkyfs
/clear/courses/comp421/pub/bin/yalnix -ly 10 yfs tests/sample1

/clear/courses/comp421/pub/bin/mkyfs
/clear/courses/comp421/pub/bin/yalnix -ly 10 yfs tests/sample2