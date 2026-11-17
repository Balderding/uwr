import torch, importlib
print('torch', torch.__version__, 'cuda', torch.cuda.is_available(), torch.cuda.get_device_name(0) if torch.cuda.is_available() else '')
for m in ('unsloth', 'trl', 'transformers', 'peft', 'datasets', 'gguf'):
    try: print(m, importlib.import_module(m).__version__)
    except Exception as e: print(m, 'FAILED:', str(e)[:200])
