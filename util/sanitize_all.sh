#!/bin/sh
#
# Copyright (C) 2000  Internet Software Consortium.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# $Id: sanitize_all.sh,v 1.6 2000/09/27 17:16:01 mws Exp $

PERL=perl5

# Run this shell script from a CVS export'ed source tree, and it will
# sanitize all of the files in that tree.

find . -name '*.[ch]' | xargs $PERL util/sanitize.pl -kPUBLIC $*
find . -name '*.in' | xargs $PERL util/sanitize.pl -kPUBLIC $*
for file in `find . -name '*.dirty'`
do
    clean=`echo $file | sed 's/\.dirty$//'`
    $PERL util/sanitize.pl -kPUBLIC - < $file > $clean
    rm $file
done

