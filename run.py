import pandas as pd
import matplotlib.pyplot as plt

# Caminho do arquivo CSV
arquivo = 'data.csv'

# Leitura do CSV
df = pd.read_csv(arquivo, sep=';', header=None, names=['ax', 'ay', 'az', 'gx', 'gy', 'gz'])

# Criar vetor de tempo
df['tempo'] = range(len(df))

# Lista de colunas para plotar
colunas = ['ax', 'ay', 'az', 'gx', 'gy', 'gz']

# Criar figura com subplots 3x2
fig, axs = plt.subplots(3, 2, figsize=(12, 8))
axs = axs.flatten()  # Transformar em lista 1D para facilitar acesso

# Gerar um subplot para cada coluna
for i, coluna in enumerate(colunas):
    axs[i].plot(df['tempo'], df[coluna], label=coluna, color='blue')
    axs[i].set_title(f'{coluna} vs Tempo')
    axs[i].set_xlabel('Tempo (amostras)')
    axs[i].set_ylabel(coluna)
    axs[i].grid(True)
    axs[i].legend()

# Ajuste de layout para evitar sobreposição
plt.tight_layout()
plt.show()
