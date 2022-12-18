#pragma once

class OFDM_Demod;
class BasicRadio;
class AudioMixer;

void RenderDABModule(OFDM_Demod& demod, BasicRadio& radio, AudioMixer& mixer);