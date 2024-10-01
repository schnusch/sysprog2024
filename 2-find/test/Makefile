DOCKER = docker

tests = \
	test-absolute \
	test-absolute-name \
	test-loop \
	test-noaccess \
	test-special-name \
	test-type-d \
	test-type-d-follow \
	test-type-f \
	test-type-f-follow \
	test-xdev

.PHONY: \
	test \
	in-docker

test: image-id
	$(DOCKER) run \
		-v ..:/host:ro \
		-v ./loop:/loop:ro \
		-v ./loop:/loop/loop:ro \
		--tmpfs /test \
		--tmpfs /test/test/test-loop.d/a/d \
		--tmpfs /test/test/test-xdev.d/a/b \
		--user user \
		"$$(cat image-id)" \
		make -C /test/test in-docker

in-docker: $(tests)

image-id: Dockerfile entrypoint.sh
	$(DOCKER) build --iidfile=.tmp.$@ -f Dockerfile .
	if ! cmp -s .tmp.$@ $@; then mv .tmp.$@ $@; fi

test-%:
	mkdir -p $@.d
	(cd $@.d && ../$@-prepare.sh) | sort > $@-expect.txt
	(cd $@.d && stdbuf -o0 ../$@-run.sh 2>&1) | sort | diff -u $@-expect.txt -
	# compare to GNU find, ignore error lines
	(cd $@.d && ../$@-find.sh) | sort > $@-find.txt
	grep -Fv ':' $@-expect.txt | diff -u - $@-find.txt
