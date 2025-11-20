# System Architecture Flowchart

This Mermaid diagram mirrors the project's boot order, runtime loop, and sensor-to-output responsibilities. VS Code users can open the built-in Markdown preview (``Ctrl+Shift+V``) or the Mermaid preview extension to see the rendered flow.

```mermaid
flowchart TD
    subgraph Boot[Boot + Setup]
        A[ESP32 reset / power-on]
        B[Arduino setup()]
        C[otaBootDispatcher]
        D[ConductManager.begin()]
    end
    A --> B --> C --> D

    subgraph Planner[Early conduct planning]
        D --> D1[TimerManager timers<br/>+ BootMaster heartbeat]
        D --> D2[HeartbeatBoot/Conduct<br/>StatusBoot/Conduct]
        D --> SDCheck{sdBoot.plan()}
    end

    SDCheck -->|fail| Abort[Retry SD / stay idle]
    SDCheck -->|ready| Resume[resumeAfterSDBoot()]

    subgraph PostSD[Post-SD bring-up]
        Resume --> SDcond[sdConduct.plan()]
        Resume --> Calendar[calendarBoot + calendarConduct]
        Resume --> WiFi[WiFiBoot + WiFiConduct]
        Resume --> Web[WebBoot + WebConduct + WebDirector]
        Resume --> Audio[audioBoot + audioConduct]
        Resume --> Light[lightBoot + lightConduct]
        Resume --> Sensors[sensorsBoot + sensorsConduct]
        Resume --> OTA[otaBoot + otaConduct]
        Resume --> Speak[speakBoot + speakConduct]
    end

    subgraph Loop[Main loop]
        L[loop()]
        TM[TimerManager::update]
        CM[ConductManager::update]
        AudioMgr[AudioManager::update]
        L --> TM --> CM --> AudioMgr
    end

    subgraph DataFlow[Sensing → Conduct → Policy → Hardware]
        SensorsMgr[SensorManager]
        SensorsPol[SensorsPolicy<br/>(normalise/filter)]
        ConductCoord[Conduct modules / intents]
        PolicyOut[Output policies<br/>(Light/Audio/Heartbeat/...)]
        HardwareMgr[Hardware managers<br/>(LightManager, AudioManager, etc.)]
        SensorsMgr --> SensorsPol --> ConductCoord --> PolicyOut --> HardwareMgr
    end

    Calendar --> ConductCoord
    SDcond --> ConductCoord
    WiFi --> Web --> ConductCoord
    OTA --> ConductCoord
    Speak --> ConductCoord
    Audio --> PolicyOut
    Light --> PolicyOut
    Sensors --> SensorsMgr
```

## Notes
- **Boot barrier:** Everything beyond `resumeAfterSDBoot()` waits until the SD card is mounted so shared CSV/config data is available.
- **Timers as triggers:** `TimerManager` seeds hourly chimes, periodic fragments, status logs, and the fallback clock tick.
- **Layered responsibilities:** Input managers/policies only normalise data; conduct modules coordinate intent routing; output policies own behaviour rules before delegating to hardware managers.
- **Web + Wi-Fi:** The Wi-Fi stack brings connectivity online, then the Web director exposes REST controls that ultimately call Conduct intents (e.g., brightness/autoplay adjustments).
```