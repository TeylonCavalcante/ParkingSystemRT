# sim_clients.sh
# Simula N clientes
# Run: ./src/sim_clients.sh 12 (por padrão 12 clientes, altere conforme desejar)
N=${1:-12}
for i in $(seq 1 $N); do
  ./build/client &
  sleep 0.1
done
wait
echo "Simulação finalizada."