# Resumo em português

Extensão local experimental para **uma câmera XM530V200 / IPG-X2-WQ-BL testada**. Preserva o firmware e os drivers originais; não é OpenIPC e não é uma imagem universal para instalar em qualquer câmera iCSee.

Foram testados Wi-Fi, vídeo HEVC 2304 × 1296 a 12 fps, os dois eixos dos motores, LEDs brancos, microfone e bipes no alto-falante. O painel oferece consulta e listagem do cartão, agenda de gravação, movimentos curtos e iluminação. A gravação WFS voltou após dois reinícios, preservando um arquivo anterior e criando novos dados. Painel e SSH passaram a iniciar da memória interna.

No HA, o fluxo adicional H.264/Opus é convertido pelo servidor e transmitido por WebRTC. Mantém a resolução, mas a conversão não é sem perdas. As gravações originais do cartão continuam H.265. O atraso exato antes/depois não foi medido.

**Não foram implementados** reprodução/download/exclusão no painel, MQTT, nova IA, timelapse ou interface completa de interfone. A remoção periódica de rotas padrão **não equivale a firewall**; não afirmamos ter eliminado todo código ou toda possibilidade de comunicação do fabricante.

Esta publicação contém código-fonte, exemplos e documentação sanitizados. Não contém dumps, imagens proprietárias, senhas, chaves, configurações Wi-Fi, filmagens da casa ou instalador de flash. Consulte os documentos em inglês para build, API, testes e limitações. O trabalho teve assistência de IA, com observações confirmadas no equipamento pelo proprietário.
