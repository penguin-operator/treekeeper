export LIB=(ncursesw)
export BIN=treekeeper

OLD_PS1=$PS1
PS1="(make.sh) $OLD_PS1"

build () {
	gcc -o $BIN src/*.c `for i in ${LIB[@]}; do echo -n "-l$i "; done`
}

run () {
	./$BIN
	reset
}

envexit () {
	PS1=$OLD_PS1
	unset OLD_PS1
	unset LIB
	unset BIN
	unset -f build
	unset -f run
	unset -f envexit
	unset -f envreset
}

envreset () {
	envexit
	source make.sh
	reset
}
