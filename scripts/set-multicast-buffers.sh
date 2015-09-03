#/bin/bash
sudo sysctl -w net.core.rmem_max=2097152
sudo sysctl -w net.core.rmem_default=2097152
sudo sysctl -w net.core.wmem_max=2097152
sudo sysctl -w net.core.wmem_default=2097152
