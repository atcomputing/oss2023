GREP1 = sudo grep -R somepattern ~/Downloads > /dev/null 2>&1
FIND1 = sudo find / -mtime 0 > /dev/null 2>&1
DDD1  = dd if=/dev/zero of=/dev/shm/big bs=1M count=4096
CAT1  = cat /dev/shm/big > /dev/null
GREP2 = grep ^ /proc/*/oom_score
GREP3 = grep ^ /proc/*/oom_score | sort -t: -k2n
POD1  = podman run  --memory=50M  atcomp/perfload  usemem -r 2 20M 20M

nothing:# ignore make without parameter

grep:
	@echo -n NEXT: "$(GREP1) "
	@read
	@$(GREP1) || true

find:
	@echo -n NEXT: "$(FIND1) "
	@read
	@$(FIND1) || true

dd:
	@echo -n NEXT: "$(DDD1) "
	@read
	@$(DDD1)

cat:
	@echo -n NEXT: "$(CAT1) "
	@read
	@$(CAT1)

score1:
	@echo -n NEXT: "$(GREP2) "
	@read
	@$(GREP2)

score2:
	@echo -n NEXT: "$(GREP3) "
	@read
	@$(GREP3)

podman:
	@echo -n NEXT: "$(POD1) "
	@read
	@$(POD1)

clean:
	podman container prune
	rm /dev/shm/big 2> /dev/null || true
	sync
	sudo su -c 'echo 3 > /proc/sys/vm/drop_caches'
