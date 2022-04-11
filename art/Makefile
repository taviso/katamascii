
.PHONY: clean

all: $(patsubst %.ans,%.utf,$(wildcard *.ans))

%.utf: %.ans
	tr -d '\r' < $^                                \
		| grep --text -v '^$$'                     \
		| grep --text -v -P '^\e\[0m\e\[255D$$'    \
		| iconv -t UTF-8 -f CP437                  \
		| perl -pe 'chomp if eof' > $@

clean:
	rm -f *.utf
