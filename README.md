# Aurora Uplift VST2 Only

Versão reescrita do Aurora Uplift para gerar **apenas VST2 de 64 bits para Windows**.

## Saída

A compilação gera um único arquivo:

```text
Aurora Uplift.dll
```

## Recursos

- Sintetizador supersaw polifônico de 16 vozes
- Unison de 1 a 9 vozes por nota
- Detune e abertura estéreo
- Segundo oscilador, sub e noise
- Envelopes de amplitude e filtro
- Filtro low-pass ressonante
- Trance gate sincronizado ao BPM
- Delay ping-pong sincronizado
- Reverb estéreo
- Pitch bend
- 32 parâmetros automatizáveis
- Runtime do Visual C++ ligado estaticamente
- Sem VST3 e sem pasta `.vst3`

## Compilar no GitHub Actions

1. Crie um repositório novo ou apague o conteúdo do repositório antigo.
2. Envie **todo o conteúdo desta pasta**, incluindo `.github`.
3. Abra **Actions > Build Windows VST2 DLL**.
4. Clique em **Run workflow**.
5. Quando ficar verde, abra a execução e baixe o artefato:

```text
Aurora-Uplift-Windows-VST2
```

## Instalar no FL Studio

1. Crie uma pasta, por exemplo:

```text
C:\VSTPlugins\Aurora Uplift\
```

2. Copie para ela:

```text
Aurora Uplift.dll
```

3. No FL Studio abra **Options > Manage plugins**.
4. Em **Plugin search paths**, adicione:

```text
C:\VSTPlugins
```

5. Ative **Verify plugins**, **Rescan previously verified plugins** e **Rescan plugins with errors**.
6. Clique em **Find installed plugins**.
7. Pesquise por **Aurora Uplift**.

## Interface

Esta primeira versão VST2-only não possui editor gráfico próprio. O FL Studio expõe os 32 parâmetros pelo wrapper/editor genérico e pela opção **Browse parameters**. Isso reduz dependências e prioriza o reconhecimento da DLL.

## Créditos técnicos

O projeto utiliza o DISTRHO Plugin Framework (DPF). Consulte `NOTICE-DPF.txt`.
