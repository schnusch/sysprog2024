DOCKER = docker

tests = \
	test-noaccess \
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
		--tmpfs /test \
		--tmpfs /test/test/test-xdev.d/a/b \
		--user user \
		"$$(cat image-id)" \
		sh -ec 'PS4="\$$ "; set -x; sudo chown -R user:users /test; cp -a /host/* /test && make -C /test clean all && make -C /test/test in-docker'

in-docker: $(tests)

image-id: Dockerfile
	$(DOCKER) build --iidfile=.tmp.$@ -f Dockerfile .
	if ! cmp -s .tmp.$@ $@; then mv .tmp.$@ $@; fi

test-%:
	mkdir -p $@.d
	(cd $@.d && ../$@-prepare.sh) | sort > $@-expect.txt
	(cd $@.d && stdbuf -o0 ../$@-run.sh 2>&1) | sort | diff -u $@-expect.txt -
