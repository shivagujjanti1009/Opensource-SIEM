import sys
sys.path.append('/tools')

from healthcheck_utils import get_manager_health_base


if __name__ == "__main__":
    exit(get_manager_health_base(docker_compose_file=sys.argv[1] if len(sys.argv) > 1 else None))
