from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
text=(root/'web/panel.html').read_text(encoding='utf-8')
(root/'src/panel.h').write_text('/* Generated from web/panel.html. */\nstatic const char panel_html[] = '+json.dumps(text,ensure_ascii=True)+';\n',encoding='utf-8')
