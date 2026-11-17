// Generated explicit unsupported overrides for the A/W interface template.
HRESULT STDMETHODCALLTYPE EnumObjects(EnumCallback lpCallback, LPVOID pvRef, DWORD dwFlags) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE SetEventNotification(HANDLE hEvent) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE GetObjectInfo(ObjectInfo pdidoi, DWORD dwObj, DWORD dwHow) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE GetDeviceInfo(DeviceInfo pdidi) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE RunControlPanel(HWND hwndOwner, DWORD dwFlags) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE hinst, DWORD dwVersion, REFGUID rguid) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID rguid, LPCDIEFFECT lpeff, LPDIRECTINPUTEFFECT *ppdeff, LPUNKNOWN punkOuter) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE EnumEffects(EffectCallback lpCallback, LPVOID pvRef, DWORD dwEffType) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE GetEffectInfo(EffectInfo pdei, REFGUID rguid) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE GetForceFeedbackState(LPDWORD pdwOut) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE SendForceFeedbackCommand(DWORD dwFlags) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK lpCallback, LPVOID pvRef, DWORD fl) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE pesc) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE SendDeviceData(DWORD cbObjectData, LPCDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD fl) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE EnumEffectsInFile(Text lpszFileName,LPDIENUMEFFECTSINFILECALLBACK pec,LPVOID pvRef,DWORD dwFlags) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE WriteEffectToFile(Text lpszFileName,DWORD dwEntries,LPDIFILEEFFECT rgDiFileEft,DWORD dwFlags) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE BuildActionMap(ActionFormat lpdiaf, Text lpszUserName, DWORD dwFlags) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE SetActionMap(ActionFormat lpdiaf, Text lpszUserName, DWORD dwFlags) override { return DIERR_UNSUPPORTED; }
HRESULT STDMETHODCALLTYPE GetImageInfo(ImageInfo lpdiDevImageInfoHeader) override { return DIERR_UNSUPPORTED; }
