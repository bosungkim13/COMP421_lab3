#!/bin/bash

/clear/courses/comp421/pub/bin/mkyfs
/clear/courses/comp421/pub/bin/yalnix -ly 10 yfs tcreate

/clear/courses/comp421/pub/bin/mkyfs
/clear/courses/comp421/pub/bin/yalnix -ly 10 yfs tlink

/clear/courses/comp421/pub/bin/mkyfs
/clear/courses/comp421/pub/bin/yalnix -ly 10 yfs tcreate2
/clear/courses/comp421/pub/bin/yalnix -ly 10 yfs topen2
/clear/courses/comp421/pub/bin/yalnix -ly 10 yfs tunlink2

/clear/courses/comp421/pub/bin/mkyfs
/clear/courses/comp421/pub/bin/yalnix -ly 10 yfs tls

/clear/courses/comp421/pub/bin/mkyfs
/clear/courses/comp421/pub/bin/yalnix -ly 10 yfs tsymlink

/clear/courses/comp421/pub/bin/mkyfs
/clear/courses/comp421/pub/bin/yalnix -ly 10 yfs tunlink2

/clear/courses/comp421/pub/bin/mkyfs
/clear/courses/comp421/pub/bin/yalnix -ly 10 yfs writeread
