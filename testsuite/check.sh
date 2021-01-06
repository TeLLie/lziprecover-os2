#! /bin/sh
# check script for Lziprecover - Data recovery tool for the lzip format
# Copyright (C) 2009-2021 Antonio Diaz Diaz.
#
# This script is free software: you have unlimited permission
# to copy, distribute, and modify it.

LC_ALL=C
export LC_ALL
objdir=`pwd`
testdir=`cd "$1" ; pwd`
LZIP="${objdir}"/lziprecover
LZIPRECOVER="${LZIP}"
framework_failure() { echo "failure in testing framework" ; exit 1 ; }

if [ ! -f "${LZIP}" ] || [ ! -x "${LZIP}" ] ; then
	echo "${LZIP}: cannot execute"
	exit 1
fi

[ -e "${LZIP}" ] 2> /dev/null ||
	{
	echo "$0: a POSIX shell is required to run the tests"
	echo "Try bash -c \"$0 $1 $2\""
	exit 1
	}

if [ -d tmp ] ; then rm -rf tmp ; fi
mkdir tmp
cd "${objdir}"/tmp || framework_failure

cat "${testdir}"/test.txt > in || framework_failure
in_lz="${testdir}"/test.txt.lz
in_lzma="${testdir}"/test.txt.lzma
in_em="${testdir}"/test_em.txt.lz
inD="${testdir}"/test21723.txt
bad1_lz="${testdir}"/test_bad1.lz
bad2_lz="${testdir}"/test_bad2.lz
bad3_lz="${testdir}"/test_bad3.lz
bad4_lz="${testdir}"/test_bad4.lz
bad5_lz="${testdir}"/test_bad5.lz
fox_lz="${testdir}"/fox.lz
fox6_lz="${testdir}"/fox6.lz
f6b1="${testdir}"/fox6_bad1.txt
f6b1_lz="${testdir}"/fox6_bad1.lz
f6b2_lz="${testdir}"/fox6_bad2.lz
f6b3_lz="${testdir}"/fox6_bad3.lz
f6b4_lz="${testdir}"/fox6_bad4.lz
f6b5_lz="${testdir}"/fox6_bad5.lz
f6b6_lz="${testdir}"/fox6_bad6.lz
f6s1_lz="${testdir}"/fox6_sc1.lz
f6s2_lz="${testdir}"/fox6_sc2.lz
f6s3_lz="${testdir}"/fox6_sc3.lz
f6s4_lz="${testdir}"/fox6_sc4.lz
f6s5_lz="${testdir}"/fox6_sc5.lz
f6s6_lz="${testdir}"/fox6_sc6.lz
num_lz="${testdir}"/numbers.lz
nbt_lz="${testdir}"/numbersbt.lz
fail=0
test_failed() { fail=1 ; printf " $1" ; [ -z "$2" ] || printf "($2)" ; }

# Description of test files for lziprecover:
# single-member files with one or more errors
# test_bad1.lz: byte at offset 66 changed from 0xA6 to 0x26
# test_bad2.lz: [  34-  65] --> copy of bytes [  68-  99]
# test_bad3.lz: [ 512-1535] --> zeroed          [2560-3583] --> zeroed
# test_bad4.lz: [3072-4095] --> random errors   [4608-5631] --> zeroed
# test_bad5.lz: [1024-2047] --> random errors   [5120-6143] --> random data
# test_bad6.lz: [ 512-1023] --> zeroed   (reference test.txt [  891- 2137])
# test_bad7.lz: [6656-7167] --> zeroed   (reference test.txt [20428-32231])
# test_bad8.lz: [  66-  73] --> zeroed   (reference test.txt [   89-  110])
# test_bad9.lz: [6491-6498] --> zeroed   (reference test.txt [17977-18120])
#
# 6-member files with one or more errors
# fox6_bad1.lz: byte at offset   5 changed from 0x0C to 0x00 (DS)
#               byte at offset 142 changed from 0x50 to 0x70 (CRC)
#               byte at offset 224 changed from 0x2D to 0x2E (data_size)
#               byte at offset 268 changed from 0x34 to 0x33 (mid stream)
#               byte at offset 327 changed from 0x2A to 0x2B (byte 7)
#               byte at offset 458 changed from 0xA0 to 0x20 (EOS marker)
# fox6_bad2.lz: [110-129] --> zeroed  (member 2)
# fox6_bad3.lz: [180-379] --> zeroed  (members 3-5)
# fox6_bad4.lz: [330-429] --> zeroed  (members 5,6)
# fox6_bad5.lz: [380-479] --> zeroed  (members 5,6)
# fox6_bad6.lz: [430-439] --> zeroed  (member 6)
#
# 6-member files "shortcircuited" by a corrupt or fake trailer
# fox6_sc1.lz: (corrupt but consistent last trailer)
#              last CRC != 0 ; dsize = 4 * msize ; msize = 480 (file size)
# fox6_sc2.lz: (appended fake but consistent trailer)
#              fake CRC != 0 ; dsize = 4 * msize ; msize = 500 (file size)
# fox6_sc3.lz: fake CRC = 0
# fox6_sc4.lz: fake dsize = 0
# fox6_sc5.lz: fake dsize = 411 (< 8 * ( fake msize - 36 ) / 9)
# fox6_sc6.lz: fake dsize = 3360660 (>= 7090 * ( fake msize - 26 ))
#
# 9-member files "one_" "two_" "three_" "four_" "five_" "six_" "seven_"
#                "eight_" "nine_"
# numbers.lz  : good file containing the 9 members shown above
# numbersbt.lz: "gap" after "three_", "damaged" after "six_", "trailing data"

printf "testing lziprecover-%s..." "$2"

"${LZIP}" -lq in
[ $? = 2 ] || test_failed $LINENO
"${LZIP}" -tq in
[ $? = 2 ] || test_failed $LINENO
"${LZIP}" -tq < in
[ $? = 2 ] || test_failed $LINENO
"${LZIP}" -cdq in
[ $? = 2 ] || test_failed $LINENO
"${LZIP}" -cdq < in
[ $? = 2 ] || test_failed $LINENO
"${LZIP}" -dq -o in < "${in_lz}"
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" -dq -o in "${in_lz}"
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" -dq -o out nx_file.lz
[ $? = 1 ] || test_failed $LINENO
[ ! -e out ] || test_failed $LINENO
# these are for code coverage
"${LZIP}" -lt "${in_lz}" 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" -cdl "${in_lz}" > out 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" -cdt "${in_lz}" > out 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" -t -- nx_file.lz 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" -t "" < /dev/null 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" --help > /dev/null || test_failed $LINENO
"${LZIP}" -n1 -V > /dev/null || test_failed $LINENO
"${LZIP}" -m 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" -z 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" --bad_option 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" --t 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" --test=2 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" --output= 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" --output 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
printf "LZIP\001-.............................." | "${LZIP}" -t 2> /dev/null
printf "LZIP\002-.............................." | "${LZIP}" -t 2> /dev/null
printf "LZIP\001+.............................." | "${LZIP}" -t 2> /dev/null

"${LZIPRECOVER}" -eq "${testdir}"/test_bad6.lz
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -mq "${bad1_lz}"
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -Rq
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -sq
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -t --remove=damaged "${in_lz}" 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" --strip=damaged -t "${in_lz}" 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" --remove=tdata -t "${in_lz}" 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -t --strip=tdata "${in_lz}" 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --dump=tdata --strip=damaged "${in_lz}"
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" --remove=tdata --strip=damaged "${in_lz}" 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --dump=damaged
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --dump=damaged in > /dev/null
[ $? = 2 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --dump=damagedd "${in_lz}" > /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --strip=damaged
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --strip=damaged in > /dev/null
[ $? = 2 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --strip=damagedd "${in_lz}" > /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --remove=damaged
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --remove=damaged in
[ $? = 2 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --remove=damagedd "${in_lz}"
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --dump=tdata
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --dump=tdata in > /dev/null
[ $? = 2 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --dump=tdataa "${in_lz}" > /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --strip=tdata
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --strip=tdata in > /dev/null
[ $? = 2 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --strip=tdataa "${in_lz}" > /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --remove=tdata
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --remove=tdata in
[ $? = 2 ] || test_failed $LINENO
"${LZIPRECOVER}" -q --remove=tdataa "${in_lz}"
[ $? = 1 ] || test_failed $LINENO

"${LZIPRECOVER}" -Aq in
[ $? = 2 ] || test_failed $LINENO
"${LZIPRECOVER}" -Aq < in > copy.lz	# /dev/null returns 1 on OS/2
[ $? = 2 ] || test_failed $LINENO
"${LZIPRECOVER}" -Aq < "${in_lz}" > copy.lz
[ $? = 2 ] || test_failed $LINENO
"${LZIPRECOVER}" -Aq "${in_lz}"
[ $? = 1 ] || test_failed $LINENO
"${LZIPRECOVER}" -Akq "${in_lzma}"
[ $? = 1 ] || test_failed $LINENO
rm -f copy.lz || framework_failure
"${LZIPRECOVER}" -A "${in_lzma}" -o copy.lz || test_failed $LINENO
cmp "${in_lz}" copy.lz || test_failed $LINENO
"${LZIPRECOVER}" -Ac "${in_lzma}" > copy.lz || test_failed $LINENO
cmp "${in_lz}" copy.lz || test_failed $LINENO
rm -f copy.lz || framework_failure
"${LZIPRECOVER}" -A -o copy.lz < "${in_lzma}" || test_failed $LINENO
cmp "${in_lz}" copy.lz || test_failed $LINENO
"${LZIPRECOVER}" -A < "${in_lzma}" > copy.lz || test_failed $LINENO
cmp "${in_lz}" copy.lz || test_failed $LINENO
rm -f copy.lz || framework_failure
cat "${in_lzma}" > copy.lzma || framework_failure
"${LZIPRECOVER}" -Ak copy.lzma || test_failed $LINENO
cmp "${in_lz}" copy.lz || test_failed $LINENO
printf "to be overwritten" > copy.lz || framework_failure
"${LZIPRECOVER}" -Af copy.lzma || test_failed $LINENO
cmp "${in_lz}" copy.lz || test_failed $LINENO
rm -f copy.lz || framework_failure
cat "${in_lzma}" > copy.tlz || framework_failure
"${LZIPRECOVER}" -Ak copy.tlz || test_failed $LINENO
cmp "${in_lz}" copy.tar.lz || test_failed $LINENO
printf "to be overwritten" > copy.tar.lz || framework_failure
"${LZIPRECOVER}" -Af copy.tlz || test_failed $LINENO
cmp "${in_lz}" copy.tar.lz || test_failed $LINENO
rm -f copy.tar.lz || framework_failure
cat in in > in2 || framework_failure
"${LZIPRECOVER}" -A -o out2.lz - "${in_lzma}" - < "${in_lzma}" ||
	test_failed $LINENO
"${LZIP}" -cd out2.lz > copy2 || test_failed $LINENO
cmp in2 copy2 || test_failed $LINENO
rm -f out2.lz copy2 || framework_failure

printf "\ntesting decompression..."

for i in "${in_lz}" "${in_em}" ; do
	"${LZIP}" -lq "$i" || test_failed $LINENO "$i"
	"${LZIP}" -t "$i" || test_failed $LINENO "$i"
	"${LZIP}" -d "$i" -o copy || test_failed $LINENO "$i"
	cmp in copy || test_failed $LINENO "$i"
	"${LZIP}" -cd "$i" > copy || test_failed $LINENO "$i"
	cmp in copy || test_failed $LINENO "$i"
	"${LZIP}" -d "$i" -o - > copy || test_failed $LINENO "$i"
	cmp in copy || test_failed $LINENO "$i"
	"${LZIP}" -d < "$i" > copy || test_failed $LINENO "$i"
	cmp in copy || test_failed $LINENO "$i"
	rm -f copy || framework_failure
done

lines=$("${LZIP}" -tvv "${in_em}" 2>&1 | wc -l) || test_failed $LINENO
[ "${lines}" -eq 8 ] || test_failed $LINENO "${lines}"

lines=$("${LZIP}" -lvv "${in_em}" | wc -l) || test_failed $LINENO
[ "${lines}" -eq 11 ] || test_failed $LINENO "${lines}"

cat "${in_lz}" > copy.lz || framework_failure
"${LZIP}" -dk copy.lz || test_failed $LINENO
cmp in copy || test_failed $LINENO
printf "to be overwritten" > copy || framework_failure
"${LZIP}" -d copy.lz 2> /dev/null
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" -df copy.lz || test_failed $LINENO
[ ! -e copy.lz ] || test_failed $LINENO
cmp in copy || test_failed $LINENO

printf "to be overwritten" > copy || framework_failure
"${LZIP}" -df -o copy < "${in_lz}" || test_failed $LINENO
cmp in copy || test_failed $LINENO
rm -f out copy || framework_failure
"${LZIP}" -d -o ./- "${in_lz}" || test_failed $LINENO
cmp in ./- || test_failed $LINENO
rm -f ./- || framework_failure
"${LZIP}" -d -o ./- < "${in_lz}" || test_failed $LINENO
cmp in ./- || test_failed $LINENO
rm -f ./- || framework_failure

cat "${in_lz}" > anyothername || framework_failure
"${LZIP}" -dv - anyothername - < "${in_lz}" > copy 2> /dev/null ||
	test_failed $LINENO
cmp in copy || test_failed $LINENO
cmp in anyothername.out || test_failed $LINENO
rm -f copy anyothername.out || framework_failure

"${LZIP}" -lq in "${in_lz}"
[ $? = 2 ] || test_failed $LINENO
"${LZIP}" -lq nx_file.lz "${in_lz}"
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" -tq in "${in_lz}"
[ $? = 2 ] || test_failed $LINENO
"${LZIP}" -tq nx_file.lz "${in_lz}"
[ $? = 1 ] || test_failed $LINENO
"${LZIP}" -cdq in "${in_lz}" > copy
[ $? = 2 ] || test_failed $LINENO
cat copy in | cmp in - || test_failed $LINENO
"${LZIP}" -cdq nx_file.lz "${in_lz}" > copy
[ $? = 1 ] || test_failed $LINENO
cmp in copy || test_failed $LINENO
rm -f copy || framework_failure
cat "${in_lz}" > copy.lz || framework_failure
for i in 1 2 3 4 5 6 7 ; do
	printf "g" >> copy.lz || framework_failure
	"${LZIP}" -alvv copy.lz "${in_lz}" > /dev/null 2>&1
	[ $? = 2 ] || test_failed $LINENO $i
	"${LZIP}" -atvvvv copy.lz "${in_lz}" 2> /dev/null
	[ $? = 2 ] || test_failed $LINENO $i
done
"${LZIP}" -dq in copy.lz
[ $? = 2 ] || test_failed $LINENO
[ -e copy.lz ] || test_failed $LINENO
[ ! -e copy ] || test_failed $LINENO
[ ! -e in.out ] || test_failed $LINENO
"${LZIP}" -dq nx_file.lz copy.lz
[ $? = 1 ] || test_failed $LINENO
[ ! -e copy.lz ] || test_failed $LINENO
[ ! -e nx_file ] || test_failed $LINENO
cmp in copy || test_failed $LINENO

"${LZIP}" -lq "${in_lz}" "${in_lz}" || test_failed $LINENO
"${LZIP}" -t "${in_lz}" "${in_lz}" || test_failed $LINENO
"${LZIP}" -cd "${in_lz}" "${in_lz}" -o out > copy2 || test_failed $LINENO
[ ! -e out ] || test_failed $LINENO			# override -o
cmp in2 copy2 || test_failed $LINENO
rm -f copy2 || framework_failure
"${LZIP}" -d "${in_lz}" "${in_lz}" -o copy2 || test_failed $LINENO
cmp in2 copy2 || test_failed $LINENO
rm -f copy2 || framework_failure

cat "${in_lz}" "${in_lz}" > copy2.lz || framework_failure
printf "\ngarbage" >> copy2.lz || framework_failure
"${LZIP}" -tvvvv copy2.lz 2> /dev/null || test_failed $LINENO
"${LZIPRECOVER}" -aD0 -q copy2.lz
[ $? = 2 ] || test_failed $LINENO
"${LZIP}" -alq copy2.lz
[ $? = 2 ] || test_failed $LINENO
"${LZIP}" -atq copy2.lz
[ $? = 2 ] || test_failed $LINENO
"${LZIP}" -atq < copy2.lz
[ $? = 2 ] || test_failed $LINENO
"${LZIP}" -adkq copy2.lz
[ $? = 2 ] || test_failed $LINENO
[ ! -e copy2 ] || test_failed $LINENO
"${LZIP}" -adkq -o copy2 < copy2.lz
[ $? = 2 ] || test_failed $LINENO
[ ! -e copy2 ] || test_failed $LINENO
printf "to be overwritten" > copy2 || framework_failure
"${LZIP}" -df copy2.lz || test_failed $LINENO
cmp in2 copy2 || test_failed $LINENO
rm -f copy2 || framework_failure

"${LZIPRECOVER}" -D ,18000 "${in_lz}" > copy || test_failed $LINENO
"${LZIPRECOVER}" -D 18000 "${in_lz}" >> copy || test_failed $LINENO
cmp in copy || test_failed $LINENO
"${LZIPRECOVER}" -D 21723-22120 -fo copy "${in_lz}" || test_failed $LINENO
cmp "${inD}" copy || test_failed $LINENO
"${LZIPRECOVER}" -D 21723,397 "${in_lz}" > copy || test_failed $LINENO
cmp "${inD}" copy || test_failed $LINENO

printf "\ntesting bad input..."

headers='LZIp LZiP LZip LzIP LzIp LziP lZIP lZIp lZiP lzIP'
body='\001\014\000\203\377\373\377\377\300\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000$\000\000\000\000\000\000\000'
cat "${in_lz}" > int.lz
printf "LZIP${body}" >> int.lz
if "${LZIP}" -tq int.lz ; then
	for header in ${headers} ; do
		printf "${header}${body}" > int.lz	# first member
		"${LZIP}" -lq int.lz
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIP}" -tq int.lz
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIP}" -tq < int.lz
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIP}" -cdq int.lz > /dev/null
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIP}" -lq --loose-trailing int.lz
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIP}" -tq --loose-trailing int.lz
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIP}" -tq --loose-trailing < int.lz
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIP}" -cdq --loose-trailing int.lz > /dev/null
		[ $? = 2 ] || test_failed $LINENO ${header}
		cat "${in_lz}" > int.lz
		printf "${header}${body}" >> int.lz	# trailing data
		"${LZIP}" -lq int.lz
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIP}" -tq int.lz
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIP}" -tq < int.lz
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIP}" -cdq int.lz > /dev/null
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIP}" -lq --loose-trailing int.lz ||
			test_failed $LINENO ${header}
		"${LZIP}" -t --loose-trailing int.lz ||
			test_failed $LINENO ${header}
		"${LZIP}" -t --loose-trailing < int.lz ||
			test_failed $LINENO ${header}
		"${LZIP}" -cd --loose-trailing int.lz > /dev/null ||
			test_failed $LINENO ${header}
		"${LZIP}" -lq --loose-trailing --trailing-error int.lz
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIP}" -tq --loose-trailing --trailing-error int.lz
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIP}" -tq --loose-trailing --trailing-error < int.lz
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIP}" -cdq --loose-trailing --trailing-error int.lz > /dev/null
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIPRECOVER}" -q --dump=tdata int.lz > /dev/null
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIPRECOVER}" -q --strip=tdata int.lz > /dev/null
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIPRECOVER}" --dump=tdata --loose-trailing int.lz > \
			/dev/null || test_failed $LINENO ${header}
		"${LZIPRECOVER}" --strip=tdata --loose-trailing int.lz > \
			/dev/null || test_failed $LINENO ${header}
		"${LZIPRECOVER}" -q --remove=tdata int.lz
		[ $? = 2 ] || test_failed $LINENO ${header}
		"${LZIPRECOVER}" --remove=tdata --loose-trailing int.lz ||
			test_failed $LINENO ${header}
		cmp "${in_lz}" int.lz || test_failed $LINENO ${header}
	done
else
	printf "\nwarning: skipping header test: 'printf' does not work on your system."
fi
rm -f int.lz || framework_failure

for i in fox_v2.lz fox_s11.lz fox_de20.lz \
         fox_bcrc.lz fox_crc0.lz fox_das46.lz fox_mes81.lz ; do
	"${LZIP}" -tq "${testdir}"/$i
	[ $? = 2 ] || test_failed $LINENO $i
done

"${LZIP}" -cd "${fox_lz}" > fox || test_failed $LINENO
for i in fox_bcrc.lz fox_crc0.lz fox_das46.lz fox_mes81.lz ; do
	"${LZIP}" -cdq "${testdir}"/$i > out
	[ $? = 2 ] || test_failed $LINENO $i
	cmp fox out || test_failed $LINENO $i
	"${LZIPRECOVER}" -tq -i "${testdir}"/$i || test_failed $LINENO $i
	"${LZIPRECOVER}" -cdq -i "${testdir}"/$i > out || test_failed $LINENO $i
	cmp fox out || test_failed $LINENO $i
done
rm -f fox out || framework_failure

cat "${in_lz}" "${in_lz}" > in2.lz || framework_failure
cat "${in_lz}" "${in_lz}" "${in_lz}" > in3.lz || framework_failure
if dd if=in3.lz of=trunc.lz bs=14752 count=1 2> /dev/null &&
   [ -e trunc.lz ] && cmp in2.lz trunc.lz > /dev/null 2>&1 ; then
	for i in 6 20 14734 14753 14754 14755 14756 14757 14758 ; do
		dd if=in3.lz of=trunc.lz bs=$i count=1 2> /dev/null
		"${LZIP}" -lq trunc.lz
		[ $? = 2 ] || test_failed $LINENO $i
		"${LZIP}" -tq trunc.lz
		[ $? = 2 ] || test_failed $LINENO $i
		"${LZIP}" -tq < trunc.lz
		[ $? = 2 ] || test_failed $LINENO $i
		"${LZIP}" -cdq trunc.lz > out
		[ $? = 2 ] || test_failed $LINENO $i
		"${LZIP}" -dq < trunc.lz > out
		[ $? = 2 ] || test_failed $LINENO $i
	done
else
	printf "\nwarning: skipping truncation test: 'dd' does not work on your system."
fi
rm -f in3.lz trunc.lz out || framework_failure

for i in "${f6s1_lz}" "${f6s2_lz}" ; do
	lines=`"${LZIP}" -lvv "$i" | wc -l || test_failed $LINENO "$i"`
	[ "${lines}" -eq 2 ] || test_failed $LINENO "$i ${lines}"
done
for i in "${f6s3_lz}" "${f6s4_lz}" "${f6s5_lz}" "${f6s6_lz}" ; do
	lines=`"${LZIP}" -lvv "$i" | wc -l || test_failed $LINENO "$i"`
	[ "${lines}" -eq 9 ] || test_failed $LINENO "$i ${lines}"
done

cat "${in_lz}" > ingin.lz || framework_failure
printf "g" >> ingin.lz || framework_failure
cat "${in_lz}" >> ingin.lz || framework_failure
"${LZIP}" -lq ingin.lz
[ $? = 2 ] || test_failed $LINENO
"${LZIP}" -atq ingin.lz
[ $? = 2 ] || test_failed $LINENO
"${LZIP}" -atq < ingin.lz
[ $? = 2 ] || test_failed $LINENO
"${LZIP}" -acdq ingin.lz > out
[ $? = 2 ] || test_failed $LINENO
"${LZIP}" -adq < ingin.lz > out
[ $? = 2 ] || test_failed $LINENO
"${LZIPRECOVER}" -lq -i ingin.lz || test_failed $LINENO
"${LZIP}" -t ingin.lz || test_failed $LINENO
"${LZIP}" -t < ingin.lz || test_failed $LINENO
"${LZIP}" -cd ingin.lz > copy || test_failed $LINENO
cmp in copy || test_failed $LINENO
"${LZIP}" -d < ingin.lz > copy || test_failed $LINENO
cmp in copy || test_failed $LINENO
"${LZIPRECOVER}" -cd -i ingin.lz > copy2 || test_failed $LINENO
cmp in2 copy2 || test_failed $LINENO

"${LZIPRECOVER}" -D0 -q "${f6b1_lz}" -fo copy
[ $? = 2 ] || test_failed $LINENO
cmp -s "${f6b1}" copy && test_failed $LINENO
"${LZIPRECOVER}" -D0 -q "${f6b1_lz}" > copy
[ $? = 2 ] || test_failed $LINENO
cmp -s "${f6b1}" copy && test_failed $LINENO
"${LZIPRECOVER}" -D0 -iq "${f6b1_lz}" -fo copy || test_failed $LINENO
cmp "${f6b1}" copy || test_failed $LINENO
"${LZIPRECOVER}" -D0 -iq "${f6b1_lz}" > copy || test_failed $LINENO
cmp "${f6b1}" copy || test_failed $LINENO

touch empty || framework_failure
"${LZIPRECOVER}" -D0 -q ingin.lz > copy
[ $? = 2 ] || test_failed $LINENO
cmp empty copy || test_failed $LINENO
"${LZIPRECOVER}" -D0 -i ingin.lz > copy2 || test_failed $LINENO
cmp in2 copy2 || test_failed $LINENO
printf "LZIP\001+" > in2t.lz || framework_failure	# gap size < 36 bytes
cat "${in_lz}" in "${in_lz}" >> in2t.lz || framework_failure
printf "LZIP\001-" >> in2t.lz || framework_failure	# truncated member
"${LZIPRECOVER}" -D0 -iq in2t.lz > copy2 || test_failed $LINENO
cmp in2 copy2 || test_failed $LINENO
"${LZIPRECOVER}" -cd -iq in2t.lz > copy2 || test_failed $LINENO
cmp in2 copy2 || test_failed $LINENO
"${LZIPRECOVER}" -t -iq in2t.lz || test_failed $LINENO
rm -f in2 in2t.lz copy copy2 || framework_failure

printf "\ntesting --merge..."

rm -f copy.lz || framework_failure
"${LZIPRECOVER}" -m -o copy.lz "${fox6_lz}" "${f6b1_lz}" || test_failed $LINENO
[ ! -e copy.lz ] || test_failed $LINENO
"${LZIPRECOVER}" -m -o copy.lz "${f6b1_lz}" "${fox6_lz}" || test_failed $LINENO
[ ! -e copy.lz ] || test_failed $LINENO
"${LZIPRECOVER}" -m -o copy.lz "${bad1_lz}" "${bad2_lz}" "${bad1_lz}" -q
[ $? = 2 ] || test_failed $LINENO
[ ! -e copy.lz ] || test_failed $LINENO
"${LZIPRECOVER}" -m -o copy.lz "${bad1_lz}" "${bad2_lz}" "${bad2_lz}" -q
[ $? = 2 ] || test_failed $LINENO
[ ! -e copy.lz ] || test_failed $LINENO
cat "${bad2_lz}" > bad2.lz || framework_failure
"${LZIPRECOVER}" -m -o copy.lz "${bad1_lz}" "${bad2_lz}" bad2.lz -q
[ $? = 2 ] || test_failed $LINENO
[ ! -e copy.lz ] || test_failed $LINENO
rm -f bad2.lz || framework_failure
"${LZIPRECOVER}" -m -o copy.lz "${f6b1_lz}" "${f6b5_lz}" -q
[ $? = 2 ] || test_failed $LINENO
[ ! -e copy.lz ] || test_failed $LINENO
"${LZIPRECOVER}" -m -o copy.lz "${f6b3_lz}" "${f6b5_lz}" -q
[ $? = 2 ] || test_failed $LINENO
[ ! -e copy.lz ] || test_failed $LINENO
"${LZIPRECOVER}" -m -o copy.lz "${bad3_lz}" "${bad4_lz}" -q
[ $? = 2 ] || test_failed $LINENO
[ ! -e copy.lz ] || test_failed $LINENO

"${LZIPRECOVER}" -mf -o copy.lz "${f6b1_lz}" "${f6b4_lz}" || test_failed $LINENO
cmp "${fox6_lz}" copy.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o copy.lz "${f6b4_lz}" "${f6b1_lz}" || test_failed $LINENO
cmp "${fox6_lz}" copy.lz || test_failed $LINENO

for i in "${f6b1_lz}" "${f6b3_lz}" "${f6b4_lz}" "${f6b5_lz}" "${f6b6_lz}" ; do
	"${LZIPRECOVER}" -mf -o copy.lz "${f6b2_lz}" "$i" ||
		test_failed $LINENO "$i"
	cmp "${fox6_lz}" copy.lz || test_failed $LINENO "$i"
	"${LZIPRECOVER}" -mf -o copy.lz "$i" "${f6b2_lz}" ||
		test_failed $LINENO "$i"
	cmp "${fox6_lz}" copy.lz || test_failed $LINENO "$i"
done

for i in "${f6b3_lz}" "${f6b4_lz}" "${f6b5_lz}" "${f6b6_lz}" ; do
	"${LZIPRECOVER}" -mf -o copy.lz "${f6b1_lz}" "${f6b2_lz}" "$i" ||
		test_failed $LINENO "$i"
	cmp "${fox6_lz}" copy.lz || test_failed $LINENO "$i"
	"${LZIPRECOVER}" -mf -o copy.lz "${f6b1_lz}" "$i" "${f6b2_lz}" ||
		test_failed $LINENO "$i"
	cmp "${fox6_lz}" copy.lz || test_failed $LINENO "$i"
	"${LZIPRECOVER}" -mf -o copy.lz "${f6b2_lz}" "${f6b1_lz}" "$i" ||
		test_failed $LINENO "$i"
	cmp "${fox6_lz}" copy.lz || test_failed $LINENO "$i"
	"${LZIPRECOVER}" -mf -o copy.lz "${f6b2_lz}" "$i" "${f6b1_lz}" ||
		test_failed $LINENO "$i"
	cmp "${fox6_lz}" copy.lz || test_failed $LINENO "$i"
	"${LZIPRECOVER}" -mf -o copy.lz "$i" "${f6b1_lz}" "${f6b2_lz}" ||
		test_failed $LINENO "$i"
	cmp "${fox6_lz}" copy.lz || test_failed $LINENO "$i"
	"${LZIPRECOVER}" -mf -o copy.lz "$i" "${f6b2_lz}" "${f6b1_lz}" ||
		test_failed $LINENO "$i"
	cmp "${fox6_lz}" copy.lz || test_failed $LINENO "$i"
done

"${LZIPRECOVER}" -mf -o copy.lz "${f6b3_lz}" "${f6b4_lz}" "${f6b5_lz}" ||
	test_failed $LINENO
cmp "${fox6_lz}" copy.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o copy.lz "${f6b1_lz}" "${f6b3_lz}" "${f6b4_lz}" \
"${f6b5_lz}" || test_failed $LINENO
cmp "${fox6_lz}" copy.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o copy.lz "${f6b2_lz}" "${f6b3_lz}" "${f6b4_lz}" \
"${f6b5_lz}" || test_failed $LINENO
cmp "${fox6_lz}" copy.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o copy.lz "${f6b1_lz}" "${f6b2_lz}" "${f6b3_lz}" \
"${f6b4_lz}" "${f6b5_lz}" || test_failed $LINENO
cmp "${fox6_lz}" copy.lz || test_failed $LINENO

"${LZIPRECOVER}" -mf -o copy.lz "${bad1_lz}" "${bad2_lz}" || test_failed $LINENO
cmp "${in_lz}" copy.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o copy.lz "${bad2_lz}" "${bad1_lz}" || test_failed $LINENO
cmp "${in_lz}" copy.lz || test_failed $LINENO

cat "${bad1_lz}" "${in_lz}" "${bad1_lz}" "${bad1_lz}" > bad11.lz || framework_failure
cat "${bad1_lz}" "${in_lz}" "${bad2_lz}" "${in_lz}" > bad12.lz || framework_failure
cat "${bad2_lz}" "${in_lz}" "${bad2_lz}" "${bad2_lz}" > bad22.lz || framework_failure
cat "${in_lz}" "${in_lz}" "${in_lz}" "${in_lz}" > copy4.lz || framework_failure
"${LZIPRECOVER}" -mf -o out4.lz bad11.lz bad12.lz bad22.lz || test_failed $LINENO
cmp out4.lz copy4.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o out4.lz bad11.lz bad22.lz bad12.lz || test_failed $LINENO
cmp out4.lz copy4.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o out4.lz bad12.lz bad11.lz bad22.lz || test_failed $LINENO
cmp out4.lz copy4.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o out4.lz bad12.lz bad22.lz bad11.lz || test_failed $LINENO
cmp out4.lz copy4.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o out4.lz bad22.lz bad11.lz bad12.lz || test_failed $LINENO
cmp out4.lz copy4.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o out4.lz bad22.lz bad12.lz bad11.lz || test_failed $LINENO
cmp out4.lz copy4.lz || test_failed $LINENO
rm -f bad11.lz bad12.lz bad22.lz || framework_failure

for i in "${bad1_lz}" "${bad2_lz}" ; do
	for j in "${bad3_lz}" "${bad4_lz}" "${bad5_lz}" ; do
		"${LZIPRECOVER}" -mf -o copy.lz "$i" "$j" ||
			test_failed $LINENO "$i $j"
		cmp "${in_lz}" copy.lz || test_failed $LINENO "$i $j"
		"${LZIPRECOVER}" -mf -o copy.lz "$j" "$i" ||
			test_failed $LINENO "$i $j"
		cmp "${in_lz}" copy.lz || test_failed $LINENO "$i $j"
	done
done

"${LZIPRECOVER}" -mf -o copy.lz "${bad3_lz}" "${bad4_lz}" "${bad5_lz}" ||
	test_failed $LINENO
cmp "${in_lz}" copy.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o copy.lz "${bad3_lz}" "${bad5_lz}" "${bad4_lz}" ||
	test_failed $LINENO
cmp "${in_lz}" copy.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o copy.lz "${bad4_lz}" "${bad3_lz}" "${bad5_lz}" ||
	test_failed $LINENO
cmp "${in_lz}" copy.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o copy.lz "${bad4_lz}" "${bad5_lz}" "${bad3_lz}" ||
	test_failed $LINENO
cmp "${in_lz}" copy.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o copy.lz "${bad5_lz}" "${bad3_lz}" "${bad4_lz}" ||
	test_failed $LINENO
cmp "${in_lz}" copy.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o copy.lz "${bad5_lz}" "${bad4_lz}" "${bad3_lz}" ||
	test_failed $LINENO
cmp "${in_lz}" copy.lz || test_failed $LINENO

cat "${bad3_lz}" "${bad4_lz}" "${bad5_lz}" "${in_lz}" > bad345.lz || framework_failure
cat "${bad4_lz}" "${bad5_lz}" "${bad3_lz}" "${in_lz}" > bad453.lz || framework_failure
cat "${bad5_lz}" "${bad3_lz}" "${bad4_lz}" "${in_lz}" > bad534.lz || framework_failure
cat "${in_lz}" "${in_lz}" "${in_lz}" "${in_lz}" > copy4.lz || framework_failure
"${LZIPRECOVER}" -mf -o out4.lz bad345.lz bad453.lz bad534.lz ||
	test_failed $LINENO
cmp out4.lz copy4.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o out4.lz bad345.lz bad534.lz bad453.lz ||
	test_failed $LINENO
cmp out4.lz copy4.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o out4.lz bad453.lz bad345.lz bad534.lz ||
	test_failed $LINENO
cmp out4.lz copy4.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o out4.lz bad453.lz bad534.lz bad345.lz ||
	test_failed $LINENO
cmp out4.lz copy4.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o out4.lz bad534.lz bad345.lz bad453.lz ||
	test_failed $LINENO
cmp out4.lz copy4.lz || test_failed $LINENO
"${LZIPRECOVER}" -mf -o out4.lz bad534.lz bad453.lz bad345.lz ||
	test_failed $LINENO
cmp out4.lz copy4.lz || test_failed $LINENO
rm -f bad345.lz bad453.lz bad534.lz out4.lz copy4.lz || framework_failure

printf "\ntesting --repair..."

rm -f copy.lz || framework_failure
"${LZIPRECOVER}" -R -o copy.lz "${fox6_lz}" || test_failed $LINENO
[ ! -e copy.lz ] || test_failed $LINENO
"${LZIPRECOVER}" -R -o copy.lz "${bad2_lz}" -q
[ $? = 2 ] || test_failed $LINENO
[ ! -e copy.lz ] || test_failed $LINENO
"${LZIPRECOVER}" -R -o copy.lz "${bad3_lz}" -q
[ $? = 2 ] || test_failed $LINENO
[ ! -e copy.lz ] || test_failed $LINENO
"${LZIPRECOVER}" -R -o copy.lz "${bad4_lz}" -q
[ $? = 2 ] || test_failed $LINENO
[ ! -e copy.lz ] || test_failed $LINENO
"${LZIPRECOVER}" -Rf -o copy.lz "${f6b1_lz}" || test_failed $LINENO
cmp "${fox6_lz}" copy.lz || test_failed $LINENO
"${LZIPRECOVER}" -Rf -o copy.lz "${bad1_lz}" || test_failed $LINENO
cmp "${in_lz}" copy.lz || test_failed $LINENO

cat "${f6b1_lz}" > copy.tar.lz || framework_failure
"${LZIPRECOVER}" -R copy.tar.lz || test_failed $LINENO
[ -e copy_fixed.tar.lz ] || test_failed $LINENO
mv copy.tar.lz copy.lz || framework_failure
"${LZIPRECOVER}" -R copy.lz || test_failed $LINENO
[ -e copy_fixed.lz ] || test_failed $LINENO
mv copy.lz copy.tlz || framework_failure
"${LZIPRECOVER}" -R copy.tlz || test_failed $LINENO
[ -e copy_fixed.tlz ] || test_failed $LINENO
rm -f copy_fixed.tlz copy_fixed.lz copy_fixed.tar.lz copy.tlz ||
	framework_failure

printf "\ntesting --reproduce..."

if [ -z "${LZIP_NAME}" ] ; then LZIP_NAME=lzip ; fi
if /bin/sh -c "${LZIP_NAME} -s18KiB" < in > out 2> /dev/null &&
   cmp "${in_lz}" out > /dev/null 2>&1 ; then
  rm -f out || framework_failure
  "${LZIPRECOVER}" --reproduce --lzip-name="${LZIP_NAME}" -o out \
    --reference-file=foo "${in_lz}" || test_failed $LINENO "${LZIP_NAME}"
  [ ! -e out ] || test_failed $LINENO

  for i in 6 7 8 9 ; do
    for f in "${testdir}"/test_bad${i}.txt "${testdir}"/test.txt ; do
      rm -f out || framework_failure
      "${LZIPRECOVER}" -q --reproduce --lzip-name="${LZIP_NAME}" \
        --reference-file="$f" "${testdir}"/test_bad${i}.lz -o out ||
        test_failed $LINENO "${LZIP_NAME} $i $f"
      cmp "${in_lz}" out || test_failed $LINENO "${LZIP_NAME} $i $f"
      rm -f out || framework_failure
      "${LZIPRECOVER}" -q --reproduce --lzip-name="${LZIP_NAME}" \
        --reference-file="$f" "${testdir}"/test_bad${i}.lz -o out \
        --lzip-level=6 || test_failed $LINENO "${LZIP_NAME} $i $f level=6"
      cmp "${in_lz}" out || test_failed $LINENO "${LZIP_NAME} $i $f level=6"
      rm -f out || framework_failure
      "${LZIPRECOVER}" -q --reproduce --lzip-name="${LZIP_NAME}" \
        --reference-file="$f" "${testdir}"/test_bad${i}.lz -o out \
        --lzip-level=m36 || test_failed $LINENO "${LZIP_NAME} $i $f level=m36"
      cmp "${in_lz}" out || test_failed $LINENO "${LZIP_NAME} $i $f level=m36"
    done
  done

  cat "${in_lz}" "${in_lz}" "${in_lz}" "${in_lz}" > in4.lz || framework_failure
  # multimember reproduction using test_bad[6789].txt as reference
  cat "${testdir}"/test_bad6.lz "${testdir}"/test_bad7.lz \
      "${testdir}"/test_bad8.lz "${testdir}"/test_bad9.lz > mm_bad.lz ||
    framework_failure
  rm -f out || framework_failure
  for i in 6 7 8 9 ; do			# reproduce one member each time
    "${LZIPRECOVER}" -q --reproduce --lzip-name="${LZIP_NAME}" \
      --reference-file="${testdir}"/test_bad${i}.txt mm_bad.lz -o out ||
      test_failed $LINENO "${LZIP_NAME} $i"
    mv -f out mm_bad.lz
  done
  cmp in4.lz mm_bad.lz || test_failed $LINENO "${LZIP_NAME}"

  # multimember reproduction using test.txt as reference
  cat "${testdir}"/test_bad6.lz "${testdir}"/test_bad7.lz \
      "${testdir}"/test_bad8.lz "${testdir}"/test_bad9.lz > mm_bad.lz ||
    framework_failure
  rm -f out || framework_failure
  for i in 6 7 8 9 ; do			# reproduce one member each time
    "${LZIPRECOVER}" -q --reproduce --lzip-name="${LZIP_NAME}" \
      --reference-file="${testdir}"/test.txt mm_bad.lz -o out ||
      test_failed $LINENO "${LZIP_NAME} $i"
    mv -f out mm_bad.lz
  done
  cmp in4.lz mm_bad.lz || test_failed $LINENO "${LZIP_NAME}"
  rm -f in4.lz mm_bad.lz || framework_failure

  "${LZIPRECOVER}" -q --debug-reproduce=13-7356 --lzip-name="${LZIP_NAME}" \
    --reference-file="${testdir}"/test.txt "${testdir}"/test.txt.lz ||
    test_failed $LINENO "${LZIP_NAME}"

  "${LZIPRECOVER}" -q --debug-reproduce=512,5120,512 --lzip-name="${LZIP_NAME}" \
    --reference-file="${testdir}"/test.txt "${testdir}"/test.txt.lz ||
    test_failed $LINENO "${LZIP_NAME}"
else
  printf "\nwarning: skipping --reproduce test: ${LZIP_NAME} not found or not the right version."
  printf "\nTry 'make LZIP_NAME=<name_of_lzip_executable> check'."
fi

printf "\ntesting --split..."

cat "${in_lz}" "${in_lz}" "${in_lz}" "${in_lz}" "${in_lz}" "${in_lz}" \
    "${in_lz}" "${in_lz}" "${in_lz}" > in9.lz || framework_failure
"${LZIPRECOVER}" -s in9.lz || test_failed $LINENO
for i in 1 2 3 4 5 6 7 8 9 ; do
	cmp "${in_lz}" rec${i}in9.lz || test_failed $LINENO $i
	"${LZIP}" -cd rec${i}in9.lz > copy || test_failed $LINENO $i
	cmp in copy || test_failed $LINENO $i
done
cat rec*in9.lz | cmp in9.lz - || test_failed $LINENO
rm -f rec*in9.lz || framework_failure

cat in9.lz > in9t.lz || framework_failure
printf "garbage" >> in9t.lz || framework_failure
"${LZIPRECOVER}" -s in9t.lz || test_failed $LINENO
for i in 01 02 03 04 05 06 07 08 09 ; do
	cmp "${in_lz}" rec${i}in9t.lz || test_failed $LINENO $i
	"${LZIP}" -cd rec${i}in9t.lz > copy || test_failed $LINENO $i
	cmp in copy || test_failed $LINENO $i
done
[ -e rec10in9t.lz ] || test_failed $LINENO
[ ! -e rec11in9t.lz ] || test_failed $LINENO
cat rec*in9t.lz | cmp in9t.lz - || test_failed $LINENO
rm -f rec*in9t.lz in9t.lz || framework_failure

printf "LZIP\001+" > in9t.lz || framework_failure	# gap size < 36 bytes
cat "${in_lz}" "${in_lz}" "${in_lz}" in "${in_lz}" "${in_lz}" "${in_lz}" \
    "${in_lz}" "${in_lz}" "${in_lz}" in >> in9t.lz || framework_failure
"${LZIPRECOVER}" -s in9t.lz || test_failed $LINENO
for i in 02 03 04 06 07 08 09 10 11 ; do
	cmp "${in_lz}" rec${i}in9t.lz || test_failed $LINENO $i
	"${LZIP}" -cd rec${i}in9t.lz > copy || test_failed $LINENO $i
	cmp in copy || test_failed $LINENO $i
done
cmp in rec05in9t.lz || test_failed $LINENO
cmp in rec12in9t.lz || test_failed $LINENO
[ -e rec01in9t.lz ] || test_failed $LINENO
[ ! -e rec13in9t.lz ] || test_failed $LINENO
cat rec*in9t.lz | cmp in9t.lz - || test_failed $LINENO
rm -f rec*in9t.lz in9t.lz || framework_failure

cat "${in_lz}" "${in_lz}" "${in_lz}" "${in_lz}" "${in_lz}" "${in_lz}" \
    "${in_lz}" "${in_lz}" in "${in_lz}" > in9t.lz || framework_failure
printf "LZIP\001-" >> in9t.lz || framework_failure	# truncated member
"${LZIPRECOVER}" -s in9t.lz || test_failed $LINENO
for i in 01 02 03 04 05 06 07 08 10 ; do
	cmp "${in_lz}" rec${i}in9t.lz || test_failed $LINENO $i
	"${LZIP}" -cd rec${i}in9t.lz > copy || test_failed $LINENO $i
	cmp in copy || test_failed $LINENO $i
done
cmp in rec09in9t.lz || test_failed $LINENO
[ -e rec11in9t.lz ] || test_failed $LINENO
[ ! -e rec12in9t.lz ] || test_failed $LINENO
cat rec*in9t.lz | cmp in9t.lz - || test_failed $LINENO
rm -f rec*in9t.lz in9t.lz || framework_failure

cat "${in_lz}" "${in_lz}" "${in_lz}" in "${in_lz}" > in9t.lz || framework_failure
printf "LZIP\001-" >> in9t.lz || framework_failure	# truncated member
cat "${in_lz}" "${in_lz}" "${in_lz}" "${in_lz}" "${in_lz}" >> in9t.lz ||
	framework_failure
"${LZIPRECOVER}" -s in9t.lz || test_failed $LINENO
for i in 01 02 03 05 07 08 09 10 11 ; do
	cmp "${in_lz}" rec${i}in9t.lz || test_failed $LINENO $i
	"${LZIP}" -cd rec${i}in9t.lz > copy || test_failed $LINENO $i
	cmp in copy || test_failed $LINENO $i
done
cmp in rec04in9t.lz || test_failed $LINENO
[ -e rec06in9t.lz ] || test_failed $LINENO
[ ! -e rec12in9t.lz ] || test_failed $LINENO
cat rec*in9t.lz | cmp in9t.lz - || test_failed $LINENO
rm -f rec*in9t.lz in9t.lz || framework_failure

"${LZIPRECOVER}" -s "${f6b1_lz}" -o f6.lz || test_failed $LINENO
for i in 1 2 3 4 5 6 ; do
	[ -e rec${i}f6.lz ] || test_failed $LINENO
done
[ ! -e rec7f6.lz ] || test_failed $LINENO
cat rec*f6.lz | cmp "${f6b1_lz}" - || test_failed $LINENO
rm -f rec*f6.lz || framework_failure

"${LZIPRECOVER}" -s "${f6b2_lz}" -o f6.lz || test_failed $LINENO
for i in 1 3 4 5 6 ; do
	cmp "${fox_lz}" rec${i}f6.lz || test_failed $LINENO
done
[ -e rec2f6.lz ] || test_failed $LINENO
[ ! -e rec7f6.lz ] || test_failed $LINENO
cat rec*f6.lz | cmp "${f6b2_lz}" - || test_failed $LINENO
rm -f rec*f6.lz || framework_failure

"${LZIPRECOVER}" -s "${f6b3_lz}" -o f6.lz || test_failed $LINENO
for i in 1 2 4 ; do
	cmp "${fox_lz}" rec${i}f6.lz || test_failed $LINENO
done
[ -e rec3f6.lz ] || test_failed $LINENO
[ ! -e rec5f6.lz ] || test_failed $LINENO
cat rec*f6.lz | cmp "${f6b3_lz}" - || test_failed $LINENO
rm -f rec*f6.lz || framework_failure

for i in "${f6b4_lz}" "${f6b5_lz}" ; do
	"${LZIPRECOVER}" -s "$i" -o f6.lz || test_failed $LINENO
	for j in 1 2 3 4 ; do
		cmp "${fox_lz}" rec${j}f6.lz || test_failed $LINENO
	done
	[ -e rec5f6.lz ] || test_failed $LINENO
	[ ! -e rec6f6.lz ] || test_failed $LINENO
	cat rec*f6.lz | cmp "$i" - || test_failed $LINENO
	rm -f rec*f6.lz || framework_failure
done

"${LZIPRECOVER}" -s "${f6b6_lz}" -o f6.lz || test_failed $LINENO
for i in 1 2 3 4 5 ; do
	cmp "${fox_lz}" rec${i}f6.lz || test_failed $LINENO
done
[ -e rec6f6.lz ] || test_failed $LINENO
[ ! -e rec7f6.lz ] || test_failed $LINENO
cat rec*f6.lz | cmp "${f6b6_lz}" - || test_failed $LINENO
rm -f rec*f6.lz || framework_failure

"${LZIPRECOVER}" -s "${f6s1_lz}" -o f6.lz || test_failed $LINENO
for i in 1 2 3 4 5 ; do
	cmp "${fox_lz}" rec${i}f6.lz || test_failed $LINENO
done
[ -e rec6f6.lz ] || test_failed $LINENO
[ ! -e rec7f6.lz ] || test_failed $LINENO
cat rec*f6.lz | cmp "${f6s1_lz}" - || test_failed $LINENO
rm -f rec*f6.lz || framework_failure
for i in "${f6s2_lz}" "${f6s3_lz}" "${f6s4_lz}" "${f6s5_lz}" "${f6s6_lz}" ; do
	"${LZIPRECOVER}" -s "$i" -o f6.lz || test_failed $LINENO "$i"
	for j in 1 2 3 4 5 6 ; do
		cmp "${fox_lz}" rec${j}f6.lz || test_failed $LINENO "$i $j"
	done
	[ -e rec7f6.lz ] || test_failed $LINENO "$i"
	[ ! -e rec8f6.lz ] || test_failed $LINENO "$i"
	cat rec*f6.lz | cmp "$i" - || test_failed $LINENO "$i"
	rm -f rec*f6.lz || framework_failure
done

"${LZIPRECOVER}" -s ingin.lz || test_failed $LINENO
cmp "${in_lz}" rec1ingin.lz || test_failed $LINENO
cmp "${in_lz}" rec3ingin.lz || test_failed $LINENO
printf "g" | cmp rec2ingin.lz - || test_failed $LINENO
[ ! -e rec4ingin.lz ] || test_failed $LINENO
cat rec*ingin.lz | cmp ingin.lz - || test_failed $LINENO
rm -f rec*ingin.lz || framework_failure

printf "\ntesting --*=damaged..."

cat "${in_lz}" > in.lz || framework_failure
cat "${in_lz}" in > int.lz || framework_failure
"${LZIPRECOVER}" --dump=damaged in.lz > copy || test_failed $LINENO
cmp empty copy || test_failed $LINENO
"${LZIPRECOVER}" --dump=damage int.lz > copy || test_failed $LINENO
cmp empty copy || test_failed $LINENO
"${LZIPRECOVER}" --strip=damag in.lz > copy || test_failed $LINENO
cmp in.lz copy || test_failed $LINENO
"${LZIPRECOVER}" --strip=dama int.lz > copy || test_failed $LINENO
cmp int.lz copy || test_failed $LINENO
# strip trailing data from all but the last file
"${LZIPRECOVER}" --strip=dam int.lz int.lz > copy || test_failed $LINENO
cat "${in_lz}" "${in_lz}" in | cmp copy - || test_failed $LINENO
"${LZIPRECOVER}" --remove=da in.lz || test_failed $LINENO
cmp "${in_lz}" in.lz || test_failed $LINENO
"${LZIPRECOVER}" --remove=d int.lz || test_failed $LINENO
cat "${in_lz}" in | cmp int.lz - || test_failed $LINENO
rm -f in.lz int.lz || framework_failure

cat in9.lz in > in9t.lz || framework_failure
"${LZIPRECOVER}" --dump=damaged in9.lz > copy || test_failed $LINENO
cmp empty copy || test_failed $LINENO
"${LZIPRECOVER}" --dump=damaged in9t.lz > copy || test_failed $LINENO
cmp empty copy || test_failed $LINENO
"${LZIPRECOVER}" --strip=damaged in9.lz > copy || test_failed $LINENO
cmp in9.lz copy || test_failed $LINENO
"${LZIPRECOVER}" --strip=damaged in9t.lz > copy || test_failed $LINENO
cmp in9t.lz copy || test_failed $LINENO
"${LZIPRECOVER}" --remove=damaged in9t.lz || test_failed $LINENO
cat in9.lz in | cmp in9t.lz - || test_failed $LINENO
cat in9.lz > in9t.lz || framework_failure
"${LZIPRECOVER}" --remove=damaged in9t.lz || test_failed $LINENO
cmp in9.lz in9t.lz || test_failed $LINENO
rm -f in9t.lz || framework_failure

printf "LZIP\001+" > in9t.lz || framework_failure	# gap size < 36 bytes
cat "${in_lz}" "${in_lz}" "${in_lz}" in "${in_lz}" "${in_lz}" "${in_lz}" \
    "${in_lz}" "${in_lz}" "${in_lz}" >> in9t.lz || framework_failure
printf "LZIP\001-" >> in9t.lz || framework_failure	# truncated member
printf "LZIP\001+" > gaps || framework_failure
cat in >> gaps || framework_failure
printf "LZIP\001-" >> gaps || framework_failure
"${LZIPRECOVER}" --dump=damaged in9t.lz > copy || test_failed $LINENO
cmp gaps copy || test_failed $LINENO
"${LZIPRECOVER}" --strip=damaged in9t.lz > copy || test_failed $LINENO
cmp in9.lz copy || test_failed $LINENO
"${LZIPRECOVER}" --remove=damaged in9t.lz || test_failed $LINENO
cmp in9.lz in9t.lz || test_failed $LINENO
rm -f in9.lz in9t.lz gaps || framework_failure

"${LZIPRECOVER}" --dump=damaged "${f6b1_lz}" > copy || test_failed $LINENO
cmp "${f6b1_lz}" copy || test_failed $LINENO
cat "${f6b1_lz}" in > f6bt.lz || framework_failure
"${LZIPRECOVER}" --dump=damaged f6bt.lz > copy || test_failed $LINENO
cmp "${f6b1_lz}" copy || test_failed $LINENO
"${LZIPRECOVER}" -q --strip=damaged "${f6b1_lz}" > copy || test_failed $LINENO
cmp empty copy || test_failed $LINENO
"${LZIPRECOVER}" -q --strip=damaged f6bt.lz > copy || test_failed $LINENO
cmp empty copy || test_failed $LINENO
cat "${f6b1_lz}" > f6b.lz || framework_failure
"${LZIPRECOVER}" -q --remove=damaged f6b.lz
[ $? = 2 ] || test_failed $LINENO
cmp "${f6b1_lz}" f6b.lz || test_failed $LINENO
"${LZIPRECOVER}" -q --remove=damaged f6bt.lz
[ $? = 2 ] || test_failed $LINENO
cat "${f6b1_lz}" in | cmp f6bt.lz - || test_failed $LINENO
rm -f f6b.lz f6bt.lz || framework_failure

"${LZIPRECOVER}" --dump=damaged "${f6b2_lz}" > copy || test_failed $LINENO
cat "${fox_lz}" copy "${fox_lz}" "${fox_lz}" "${fox_lz}" \
    "${fox_lz}" | cmp "${f6b2_lz}" - || test_failed $LINENO
cat "${f6b2_lz}" in > f6bt.lz || framework_failure
"${LZIPRECOVER}" --dump=damaged f6bt.lz > copy || test_failed $LINENO
cat "${fox_lz}" copy "${fox_lz}" "${fox_lz}" "${fox_lz}" \
    "${fox_lz}" | cmp "${f6b2_lz}" - || test_failed $LINENO
cat "${fox_lz}" "${fox_lz}" "${fox_lz}" "${fox_lz}" "${fox_lz}" > fox5.lz
"${LZIPRECOVER}" --strip=damaged "${f6b2_lz}" > copy || test_failed $LINENO
cmp fox5.lz copy || test_failed $LINENO
"${LZIPRECOVER}" --strip=damaged f6bt.lz > copy || test_failed $LINENO
cat fox5.lz in | cmp copy - || test_failed $LINENO
cat "${f6b2_lz}" > f6b.lz || framework_failure
"${LZIPRECOVER}" --remove=damaged f6b.lz || test_failed $LINENO
cmp fox5.lz f6b.lz || test_failed $LINENO
"${LZIPRECOVER}" --remove=damaged f6bt.lz || test_failed $LINENO
cat fox5.lz in | cmp f6bt.lz - || test_failed $LINENO
rm -f f6b.lz f6bt.lz || framework_failure

"${LZIPRECOVER}" --dump=damaged "${f6b3_lz}" > copy || test_failed $LINENO
cat "${fox_lz}" "${fox_lz}" copy "${fox_lz}" | cmp "${f6b3_lz}" - ||
	test_failed $LINENO
cat "${f6b3_lz}" in > f6bt.lz || framework_failure
"${LZIPRECOVER}" --dump=damaged f6bt.lz > copy || test_failed $LINENO
cat "${fox_lz}" "${fox_lz}" copy "${fox_lz}" | cmp "${f6b3_lz}" - ||
	test_failed $LINENO
cat "${fox_lz}" "${fox_lz}" "${fox_lz}" > fox3.lz
"${LZIPRECOVER}" --strip=damaged "${f6b3_lz}" > copy || test_failed $LINENO
cmp fox3.lz copy || test_failed $LINENO
"${LZIPRECOVER}" --strip=damaged f6bt.lz > copy || test_failed $LINENO
cat fox3.lz in | cmp copy - || test_failed $LINENO
cat "${f6b3_lz}" > f6b.lz || framework_failure
"${LZIPRECOVER}" --remove=damaged f6b.lz || test_failed $LINENO
cmp fox3.lz f6b.lz || test_failed $LINENO
"${LZIPRECOVER}" --remove=damaged f6bt.lz || test_failed $LINENO
cat fox3.lz in | cmp f6bt.lz - || test_failed $LINENO
rm -f f6b.lz f6bt.lz fox3.lz || framework_failure

cat "${fox_lz}" "${fox_lz}" "${fox_lz}" "${fox_lz}" > fox4.lz
for i in "${f6b4_lz}" "${f6b5_lz}" ; do
	"${LZIPRECOVER}" --dump=damaged "$i" > copy || test_failed $LINENO "$i"
	cat fox4.lz copy | cmp "$i" - || test_failed $LINENO "$i"
	cat "$i" in > f6bt.lz || framework_failure
	"${LZIPRECOVER}" --dump=damaged f6bt.lz > copy ||
		test_failed $LINENO "$i"
	cat fox4.lz copy | cmp f6bt.lz - || test_failed $LINENO "$i"
	"${LZIPRECOVER}" --strip=damaged "$i" > copy || test_failed $LINENO "$i"
	cmp fox4.lz copy || test_failed $LINENO "$i"
	"${LZIPRECOVER}" --strip=damaged f6bt.lz > copy ||
		test_failed $LINENO "$i"
	cmp fox4.lz copy || test_failed $LINENO "$i"
	cat "$i" > f6b.lz || framework_failure
	"${LZIPRECOVER}" --remove=damaged f6b.lz || test_failed $LINENO "$i"
	cmp fox4.lz f6b.lz || test_failed $LINENO "$i"
	"${LZIPRECOVER}" --remove=damaged f6bt.lz || test_failed $LINENO "$i"
	cmp fox4.lz f6bt.lz || test_failed $LINENO "$i"
done
rm -f f6b.lz f6bt.lz fox4.lz || framework_failure

"${LZIPRECOVER}" --dump=damaged "${f6b6_lz}" > copy || test_failed $LINENO
cat fox5.lz copy | cmp "${f6b6_lz}" - || test_failed $LINENO
cat "${f6b6_lz}" in > f6bt.lz || framework_failure
"${LZIPRECOVER}" --dump=damaged f6bt.lz > copy || test_failed $LINENO
cat fox5.lz copy | cmp "${f6b6_lz}" - || test_failed $LINENO
"${LZIPRECOVER}" --strip=damaged "${f6b6_lz}" > copy || test_failed $LINENO
cmp fox5.lz copy || test_failed $LINENO
"${LZIPRECOVER}" --strip=damaged f6bt.lz > copy || test_failed $LINENO
cat fox5.lz in | cmp copy - || test_failed $LINENO
cat "${f6b6_lz}" > f6b.lz || framework_failure
"${LZIPRECOVER}" --remove=damaged f6b.lz || test_failed $LINENO
cmp fox5.lz f6b.lz || test_failed $LINENO
"${LZIPRECOVER}" --remove=damaged f6bt.lz || test_failed $LINENO
cat fox5.lz in | cmp f6bt.lz - || test_failed $LINENO
rm -f f6b.lz f6bt.lz || framework_failure

for i in "${f6s1_lz}" "${f6s2_lz}" ; do
	"${LZIPRECOVER}" --dump=damaged "$i" > copy || test_failed $LINENO "$i"
	cmp "$i" copy || test_failed $LINENO "$i"
	cat "$i" in > f6bt.lz || framework_failure
	"${LZIPRECOVER}" --dump=damaged f6bt.lz > copy ||
		test_failed $LINENO "$i"
	cmp "$i" copy || test_failed $LINENO "$i"
	"${LZIPRECOVER}" -q --strip=damaged "$i" > copy ||
		test_failed $LINENO "$i"
	cmp empty copy || test_failed $LINENO "$i"
	"${LZIPRECOVER}" -q --strip=damaged f6bt.lz > copy ||
		test_failed $LINENO "$i"
	cmp empty copy || test_failed $LINENO "$i"
	cat "$i" > f6b.lz || framework_failure
	"${LZIPRECOVER}" -q --remove=damaged f6b.lz
	[ $? = 2 ] || test_failed $LINENO "$i"
	cmp "$i" f6b.lz || test_failed $LINENO "$i"
	"${LZIPRECOVER}" -q --remove=damaged f6bt.lz
	[ $? = 2 ] || test_failed $LINENO "$i"
	cat "$i" in | cmp f6bt.lz - || test_failed $LINENO "$i"
done
rm -f f6b.lz f6bt.lz || framework_failure

for i in "${f6s3_lz}" "${f6s4_lz}" "${f6s5_lz}" "${f6s6_lz}" ; do
	"${LZIPRECOVER}" --dump=damaged "$i" > copy || test_failed $LINENO "$i"
	cmp empty copy || test_failed $LINENO "$i"
	cat "$i" in > f6bt.lz || framework_failure
	"${LZIPRECOVER}" --dump=damaged f6bt.lz > copy ||
		test_failed $LINENO "$i"
	cmp empty copy || test_failed $LINENO "$i"
	"${LZIPRECOVER}" --strip=damaged "$i" > copy || test_failed $LINENO "$i"
	cmp "$i" copy || test_failed $LINENO "$i"
	"${LZIPRECOVER}" --strip=damaged f6bt.lz > copy ||
		test_failed $LINENO "$i"
	cat "$i" in | cmp copy - || test_failed $LINENO "$i"
	cat "$i" > f6b.lz || framework_failure
	"${LZIPRECOVER}" --remove=damaged f6b.lz || test_failed $LINENO "$i"
	cmp "$i" f6b.lz || test_failed $LINENO "$i"
	"${LZIPRECOVER}" --remove=damaged f6bt.lz || test_failed $LINENO "$i"
	cat "$i" in | cmp f6bt.lz - || test_failed $LINENO "$i"
done
rm -f f6b.lz f6bt.lz || framework_failure

cat ingin.lz "${inD}" > ingint.lz || framework_failure
"${LZIPRECOVER}" --dump=damaged ingin.lz > copy || test_failed $LINENO
printf "g" | cmp copy - || test_failed $LINENO
"${LZIPRECOVER}" --dump=damaged ingint.lz > copy || test_failed $LINENO
printf "g" | cmp copy - || test_failed $LINENO
"${LZIPRECOVER}" --strip=damaged ingin.lz > copy || test_failed $LINENO
cmp in2.lz copy || test_failed $LINENO
"${LZIPRECOVER}" --strip=damaged ingint.lz > copy || test_failed $LINENO
cat "${in_lz}" "${in_lz}" "${inD}" | cmp copy - || test_failed $LINENO
cat ingin.lz > ingin2.lz || framework_failure
"${LZIPRECOVER}" --remove=damaged ingin2.lz || test_failed $LINENO
cmp in2.lz ingin2.lz || test_failed $LINENO
"${LZIPRECOVER}" --remove=damaged ingint.lz || test_failed $LINENO
cat "${in_lz}" "${in_lz}" "${inD}" | cmp ingint.lz - || test_failed $LINENO
rm -f ingin2.lz ingint.lz || framework_failure

# concatenate output from several files
"${LZIPRECOVER}" --dump=damaged "${f6b2_lz}" > copy || test_failed $LINENO
"${LZIPRECOVER}" --dump=damaged "${bad2_lz}" "${f6b2_lz}" > copy2 ||
	test_failed $LINENO
cat "${bad2_lz}" copy | cmp copy2 - || test_failed $LINENO
cat "${bad2_lz}" in > bad2t.lz || framework_failure
cat "${f6b2_lz}" in > f6bt.lz || framework_failure
"${LZIPRECOVER}" --dump=damaged bad2t.lz "${f6b2_lz}" "${bad2_lz}" \
f6bt.lz > copy4 || test_failed $LINENO
cat "${bad2_lz}" copy "${bad2_lz}" copy | cmp copy4 - || test_failed $LINENO
"${LZIPRECOVER}" --dump=damaged "${f6b2_lz}" bad2t.lz f6bt.lz \
"${bad2_lz}" > copy4 || test_failed $LINENO
cat copy "${bad2_lz}" copy "${bad2_lz}" | cmp copy4 - || test_failed $LINENO
#
"${LZIPRECOVER}" -q --strip=damaged "${bad2_lz}" "${f6b2_lz}" > copy ||
	test_failed $LINENO
cmp fox5.lz copy || test_failed $LINENO
"${LZIPRECOVER}" -q --strip=damaged bad2t.lz "${f6b2_lz}" > copy ||
	test_failed $LINENO
cmp fox5.lz copy || test_failed $LINENO
"${LZIPRECOVER}" -q --strip=damaged "${f6b2_lz}" bad2t.lz f6bt.lz > copy ||
	test_failed $LINENO
cat fox5.lz fox5.lz in | cmp copy - || test_failed $LINENO
"${LZIPRECOVER}" -q --strip=damaged "${f6b2_lz}" f6bt.lz bad2t.lz > copy ||
	test_failed $LINENO
cat fox5.lz fox5.lz | cmp copy - || test_failed $LINENO
"${LZIPRECOVER}" -q --strip=damaged f6bt.lz bad2t.lz > copy ||
	test_failed $LINENO
cmp fox5.lz copy || test_failed $LINENO
"${LZIPRECOVER}" -q --strip=damaged f6bt.lz "${in_lz}" > copy ||
	test_failed $LINENO
cat fox5.lz "${in_lz}" | cmp copy - || test_failed $LINENO
"${LZIPRECOVER}" --strip=damaged --strip=tdata f6bt.lz "${in_lz}" > copy ||
	test_failed $LINENO
cat fox5.lz "${in_lz}" | cmp copy - || test_failed $LINENO
#
cat "${f6b2_lz}" > f6b.lz || framework_failure
"${LZIPRECOVER}" -q --remove=damaged f6b.lz bad2t.lz f6bt.lz
[ $? = 2 ] || test_failed $LINENO
cat "${bad2_lz}" in | cmp bad2t.lz - || test_failed $LINENO
cmp fox5.lz f6b.lz || test_failed $LINENO
cat fox5.lz in | cmp f6bt.lz - || test_failed $LINENO
cat "${bad2_lz}" in > bad2t.lz || framework_failure
cat "${fox6_lz}" "${inD}" > fox6t.lz || framework_failure
cat "${f6b1_lz}" in > f6abt.lz || framework_failure
cat "${f6b2_lz}" > f6b.lz || framework_failure
cat "${f6b2_lz}" in > f6bt.lz || framework_failure
"${LZIPRECOVER}" -q --remove=d:t fox6t.lz f6abt.lz f6b.lz bad2t.lz f6bt.lz
[ $? = 2 ] || test_failed $LINENO
cat "${bad2_lz}" in | cmp bad2t.lz - || test_failed $LINENO
cat "${f6b1_lz}" in | cmp f6abt.lz - || test_failed $LINENO
cmp "${fox6_lz}" fox6t.lz || test_failed $LINENO
cmp fox5.lz f6b.lz || test_failed $LINENO
cmp fox5.lz f6bt.lz || test_failed $LINENO
rm -f fox6t.lz f6b.lz f6bt.lz bad2t.lz fox5.lz copy2 copy4 || framework_failure

printf "\ntesting trailing data..."

cat "${in_lz}" "${inD}" > int.lz || framework_failure
"${LZIPRECOVER}" --dump=tdata int.lz > copy || test_failed $LINENO
cmp "${inD}" copy || test_failed $LINENO
rm -f copy || framework_failure
"${LZIPRECOVER}" --dump=tdat int.lz -o copy || test_failed $LINENO
cmp "${inD}" copy || test_failed $LINENO
cat "${fox6_lz}" "${inD}" > fox6t.lz || framework_failure
cat "${inD}" "${inD}" > inD2 || framework_failure
"${LZIPRECOVER}" --dump=tda int.lz fox6t.lz -f -o copy || test_failed $LINENO
cmp inD2 copy || test_failed $LINENO
rm -f inD2 || framework_failure
cat ingin.lz "${inD}" > ingint.lz || framework_failure
"${LZIPRECOVER}" -q --dump=td ingint.lz > /dev/null
[ $? = 2 ] || test_failed $LINENO
"${LZIPRECOVER}" -i --dump=t ingint.lz > copy || test_failed $LINENO
cmp "${inD}" copy || test_failed $LINENO

"${LZIPRECOVER}" --strip=tdata int.lz > copy || test_failed $LINENO
cmp "${in_lz}" copy || test_failed $LINENO
rm -f copy || framework_failure
"${LZIPRECOVER}" --strip=tdata int.lz -o copy || test_failed $LINENO
cmp "${in_lz}" copy || test_failed $LINENO
"${LZIPRECOVER}" --strip=tdata fox6t.lz -f -o copy || test_failed $LINENO
cmp "${fox6_lz}" copy || test_failed $LINENO
"${LZIPRECOVER}" --strip=tdata int.lz int.lz -f -o copy || test_failed $LINENO
cmp in2.lz copy || test_failed $LINENO
rm -f in2.lz || framework_failure
"${LZIPRECOVER}" --strip=tdata int.lz fox6t.lz > copy || test_failed $LINENO
cat "${in_lz}" "${fox6_lz}" | cmp copy - || test_failed $LINENO
"${LZIPRECOVER}" -q --strip=tdata ingint.lz > /dev/null
[ $? = 2 ] || test_failed $LINENO
"${LZIPRECOVER}" -i --strip=tdata ingint.lz > copy || test_failed $LINENO
cmp ingin.lz copy || test_failed $LINENO

"${LZIPRECOVER}" --remove=tdata int.lz fox6t.lz || test_failed $LINENO
cmp "${in_lz}" int.lz || test_failed $LINENO
cmp "${fox6_lz}" fox6t.lz || test_failed $LINENO
"${LZIPRECOVER}" --remove=tdata int.lz || test_failed $LINENO
cmp "${in_lz}" int.lz || test_failed $LINENO
"${LZIPRECOVER}" --remove=tdata fox6t.lz || test_failed $LINENO
cmp "${fox6_lz}" fox6t.lz || test_failed $LINENO
"${LZIPRECOVER}" -q --remove=tdata ingint.lz
[ $? = 2 ] || test_failed $LINENO
cmp -s ingin.lz ingint.lz && test_failed $LINENO
"${LZIPRECOVER}" -i --remove=tdata ingint.lz || test_failed $LINENO
cmp ingin.lz ingint.lz || test_failed $LINENO
rm -f int.lz fox6t.lz ingint.lz ingin.lz || framework_failure

for i in "${f6s3_lz}" "${f6s4_lz}" "${f6s5_lz}" "${f6s6_lz}" ; do
	"${LZIPRECOVER}" --strip=tdata "$i" > copy || test_failed $LINENO "$i"
	"${LZIPRECOVER}" --dump=tdata "$i" > tdata || test_failed $LINENO "$i"
	cmp "${fox6_lz}" copy || test_failed $LINENO "$i"
	cat copy tdata | cmp "$i" - || test_failed $LINENO "$i"
	cat "$i" "${inD}" > f6t.lz || framework_failure
	"${LZIPRECOVER}" --strip=tdata f6t.lz > copy || test_failed $LINENO "$i"
	"${LZIPRECOVER}" --dump=tdata f6t.lz > tdata || test_failed $LINENO "$i"
	cmp "${fox6_lz}" copy || test_failed $LINENO "$i"
	cat copy tdata | cmp f6t.lz - || test_failed $LINENO "$i"
	"${LZIPRECOVER}" --remove=tdata f6t.lz || test_failed $LINENO "$i"
	cmp "${fox6_lz}" f6t.lz || test_failed $LINENO "$i"
	rm -f copy tdata f6t.lz || framework_failure
done

printf "\ntesting --dump/remove/strip..."

"${LZIPRECOVER}" -s "${num_lz}" -o num.lz || test_failed $LINENO
[ -e rec9num.lz ] || test_failed $LINENO
[ ! -e rec10num.lz ] || test_failed $LINENO
cat rec*num.lz | cmp "${num_lz}" - || test_failed $LINENO
for i in 1 2 3 4 5 6 7 8 9 ; do
	"${LZIPRECOVER}" --dump=$i "${num_lz}" | cmp rec${i}num.lz - ||
		test_failed $LINENO $i
	"${LZIPRECOVER}" --strip=^$i "${num_lz}" | cmp rec${i}num.lz - ||
		test_failed $LINENO $i
	cat "${num_lz}" > num.lz || framework_failure
	"${LZIPRECOVER}" --remove=^$i num.lz || test_failed $LINENO $i
	cmp rec${i}num.lz num.lz || test_failed $LINENO $i
done
"${LZIPRECOVER}" -q --dump=1 in "${num_lz}" > out
[ $? = 2 ] || test_failed $LINENO
cmp rec1num.lz out || test_failed $LINENO
"${LZIPRECOVER}" -q --strip=^1 in "${num_lz}" > out
[ $? = 2 ] || test_failed $LINENO
cmp rec1num.lz out || test_failed $LINENO

"${LZIPRECOVER}" --dump=r1 "${num_lz}" | cmp rec9num.lz - ||
	test_failed $LINENO
"${LZIPRECOVER}" --dump=d:r3 "${num_lz}" | cmp rec7num.lz - ||
	test_failed $LINENO
"${LZIPRECOVER}" --dump=r5:d "${num_lz}" | cmp rec5num.lz - ||
	test_failed $LINENO
"${LZIPRECOVER}" --dump=t:r9 "${num_lz}" | cmp rec1num.lz - ||
	test_failed $LINENO
"${LZIPRECOVER}" --strip=r^1:t "${num_lz}" | cmp rec9num.lz - ||
	test_failed $LINENO
"${LZIPRECOVER}" --strip=d:r^3:t "${num_lz}" | cmp rec7num.lz - ||
	test_failed $LINENO
"${LZIPRECOVER}" --strip=r^5:d:t "${num_lz}" | cmp rec5num.lz - ||
	test_failed $LINENO
"${LZIPRECOVER}" --strip=d:t:r^9 "${num_lz}" | cmp rec1num.lz - ||
	test_failed $LINENO

"${LZIPRECOVER}" --dump=1,5 "${num_lz}" > out || test_failed $LINENO
cat rec1num.lz rec5num.lz | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" --dump=3,6 "${num_lz}" > out || test_failed $LINENO
cat rec3num.lz rec6num.lz | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" --dump=2-4 "${num_lz}" > out || test_failed $LINENO
cat rec2num.lz rec3num.lz rec4num.lz | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" --dump=4,6,8 "${num_lz}" > out || test_failed $LINENO
cat rec4num.lz rec6num.lz rec8num.lz | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" --strip=^1,5 "${num_lz}" > out || test_failed $LINENO
cat rec1num.lz rec5num.lz | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" --strip=^3,6 "${num_lz}" > out || test_failed $LINENO
cat rec3num.lz rec6num.lz | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" --strip=^2-4 "${num_lz}" > out || test_failed $LINENO
cat rec2num.lz rec3num.lz rec4num.lz | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" --strip=^4,6,8 "${num_lz}" > out || test_failed $LINENO
cat rec4num.lz rec6num.lz rec8num.lz | cmp out - || test_failed $LINENO

# create a subset tarlz archive
"${LZIPRECOVER}" --dump=1-2:r1:t "${num_lz}" > out || test_failed $LINENO
cat rec1num.lz rec2num.lz rec9num.lz | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" --dump=4-5:r1:t "${num_lz}" > out || test_failed $LINENO
cat rec4num.lz rec5num.lz rec9num.lz | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" --dump=7-8:r1:t "${num_lz}" > out || test_failed $LINENO
cat rec7num.lz rec8num.lz rec9num.lz | cmp out - || test_failed $LINENO

"${LZIPRECOVER}" --dump=1-9 "${num_lz}" | cmp "${num_lz}" - ||
	test_failed $LINENO
"${LZIPRECOVER}" --dump=r1-9 "${num_lz}" | cmp "${num_lz}" - ||
	test_failed $LINENO
"${LZIPRECOVER}" --dump=1-1000 "${num_lz}" | cmp "${num_lz}" - ||
	test_failed $LINENO
"${LZIPRECOVER}" --dump=r1-1000 "${num_lz}" | cmp "${num_lz}" - ||
	test_failed $LINENO
"${LZIPRECOVER}" --dump=1-4:r1-4:5 "${num_lz}" | cmp "${num_lz}" - ||
	test_failed $LINENO
"${LZIPRECOVER}" --dump=^10 "${num_lz}" | cmp "${num_lz}" - ||
	test_failed $LINENO
"${LZIPRECOVER}" --strip=^1-9 "${num_lz}" | cmp "${num_lz}" - ||
	test_failed $LINENO
"${LZIPRECOVER}" --strip=r^1-9 "${num_lz}" | cmp "${num_lz}" - ||
	test_failed $LINENO
"${LZIPRECOVER}" --strip=^1-1000 "${num_lz}" | cmp "${num_lz}" - ||
	test_failed $LINENO
"${LZIPRECOVER}" --strip=r^1-1000 "${num_lz}" | cmp "${num_lz}" - ||
	test_failed $LINENO
"${LZIPRECOVER}" --strip=^1-4:r^1-4:^5 "${num_lz}" | cmp "${num_lz}" - ||
	test_failed $LINENO
"${LZIPRECOVER}" --strip=10 "${num_lz}" | cmp "${num_lz}" - ||
	test_failed $LINENO

"${LZIPRECOVER}" -i --dump=r1 "${nbt_lz}" | cmp rec9num.lz - ||
	test_failed $LINENO
"${LZIPRECOVER}" -i --dump=r3 "${nbt_lz}" | cmp rec7num.lz - ||
	test_failed $LINENO
"${LZIPRECOVER}" -i --dump=r7 "${nbt_lz}" | cmp rec4num.lz - ||
	test_failed $LINENO
"${LZIPRECOVER}" -i --strip=r^1:t "${nbt_lz}" | cmp rec9num.lz - ||
	test_failed $LINENO
"${LZIPRECOVER}" -i --strip=r^3:t "${nbt_lz}" | cmp rec7num.lz - ||
	test_failed $LINENO
"${LZIPRECOVER}" -i --strip=r^7:t "${nbt_lz}" | cmp rec4num.lz - ||
	test_failed $LINENO

"${LZIPRECOVER}" -i --dump=4 -f -o out "${nbt_lz}" || test_failed $LINENO
printf "gap" | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" -i --dump=8 "${nbt_lz}" > out || test_failed $LINENO
printf "damaged" | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" -i --dump=tdata "${nbt_lz}" > out || test_failed $LINENO
printf "trailing data" | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" -i --dump=4:t "${nbt_lz}" > out || test_failed $LINENO
printf "gaptrailing data" | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" -i --dump=4,8:t "${nbt_lz}" > out || test_failed $LINENO
printf "gapdamagedtrailing data" | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" -i --dump=4,8 "${nbt_lz}" > out || test_failed $LINENO
printf "gapdamaged" | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" -i --dump=damaged "${nbt_lz}" > out || test_failed $LINENO
printf "gapdamaged" | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" -i --dump=d:t "${nbt_lz}" > out || test_failed $LINENO
printf "gapdamagedtrailing data" | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" -i --strip=^4:t -f -o out "${nbt_lz}" || test_failed $LINENO
printf "gap" | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" -i --strip=^8:t "${nbt_lz}" > out || test_failed $LINENO
printf "damaged" | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" -i --strip=1-11 "${nbt_lz}" > out || test_failed $LINENO
cmp empty out || test_failed $LINENO
"${LZIPRECOVER}" -i --strip=^4 "${nbt_lz}" > out || test_failed $LINENO
printf "gaptrailing data" | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" -i --strip=^4,8 "${nbt_lz}" > out || test_failed $LINENO
printf "gapdamagedtrailing data" | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" -i --strip=^4,8:t "${nbt_lz}" > out || test_failed $LINENO
printf "gapdamaged" | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" -i --strip=r^4,8:t "${nbt_lz}" > out || test_failed $LINENO
printf "gapdamaged" | cmp out - || test_failed $LINENO
"${LZIPRECOVER}" -i --strip=r^4,8 "${nbt_lz}" > out || test_failed $LINENO
printf "gapdamagedtrailing data" | cmp out - || test_failed $LINENO

cat "${num_lz}" > num.lz || framework_failure
"${LZIPRECOVER}" --remove=1-3,5,7,9 num.lz || test_failed $LINENO
cat rec4num.lz rec6num.lz rec8num.lz | cmp num.lz - || test_failed $LINENO
cat "${num_lz}" > num.lz || framework_failure
"${LZIPRECOVER}" --remove=^4,6,8 num.lz || test_failed $LINENO
cat rec4num.lz rec6num.lz rec8num.lz | cmp num.lz - || test_failed $LINENO
cat "${num_lz}" > num.lz || framework_failure
"${LZIPRECOVER}" --remove=r1,3,5,7-9 num.lz || test_failed $LINENO
cat rec4num.lz rec6num.lz rec8num.lz | cmp num.lz - || test_failed $LINENO
cat "${num_lz}" > num.lz || framework_failure
"${LZIPRECOVER}" --remove=r^2,4,6 num.lz || test_failed $LINENO
cat rec4num.lz rec6num.lz rec8num.lz | cmp num.lz - || test_failed $LINENO

cat "${nbt_lz}" > nbt.lz || framework_failure
"${LZIPRECOVER}" -i --remove=4,8:tdata nbt.lz || test_failed $LINENO
cmp "${num_lz}" nbt.lz || test_failed $LINENO
cat "${nbt_lz}" > nbt.lz || framework_failure
"${LZIPRECOVER}" -i --remove=r4,8:tdata nbt.lz || test_failed $LINENO
cmp "${num_lz}" nbt.lz || test_failed $LINENO
cat "${nbt_lz}" > nbt.lz || framework_failure
"${LZIPRECOVER}" --remove=damaged:tdata nbt.lz || test_failed $LINENO
cmp "${num_lz}" nbt.lz || test_failed $LINENO
rm -f rec*num.lz nbt.lz empty || framework_failure

for i in 1 2 3 4 5 6 7 8 9 10 ; do
	"${LZIPRECOVER}" -i --strip=1-$i "${nbt_lz}" > out ||
		test_failed $LINENO $i
	cat "${nbt_lz}" > nbt.lz || framework_failure
	"${LZIPRECOVER}" -i --remove=1-$i nbt.lz || test_failed $LINENO $i
	cmp nbt.lz out || test_failed $LINENO $i
done
rm -f nbt.lz out || framework_failure

echo
if [ ${fail} = 0 ] ; then
	echo "tests completed successfully."
	cd "${objdir}" && rm -r tmp
else
	echo "tests failed."
fi
exit ${fail}
