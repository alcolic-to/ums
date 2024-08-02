Building
-----

```bash
docker build -t docker_env --platform linux/amd64 .
```

Running
-----

```bash
docker run -itd --platform linux/amd64 docker_env
```

Attaching
-----

```bash
docker exec -it <container_id> /bin/bash
```

Misc
-----

* Remove all images: `docker image prune -a`
* Remove image: `docker rm <image_id> -f`
* Remove container: `docker rmi <container_id> -f`
* List all images: `docker images`
* List all containers: `docker ps`
