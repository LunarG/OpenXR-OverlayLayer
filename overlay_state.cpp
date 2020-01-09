struct OverlayAppSession
{
    enum OverlaySessionState {
        IDLE,
        READY,
        SYNCHRONIZED,
        VISIBLE,
        FOCUSED,
        STOPPING,
        EXITING,
        LOSS_PENDING,
        STOPPING_SYNCHRONIZED,
        STOPPING_VISIBLE,
        STOPPING_FOCUSED,
    } state;
    enum OpenXRCommand {
        BEGIN_SESSION,
        END_SESSION,
        REQUEST_EXIT_SESSION,
    };
    bool overlaySessionRunning;
    enum SessionLossState {
        NOT_LOST,
        LOSS_PENDING,
        LOST,
    } sessionLossState;
    XrSessionState currentOverlayXrSessionState;

    OverlayAppSession() :
        state(IDLE),
        currentOverlayXrSessionState(XR_SESSION_STATE_IDLE),
        overlaySessionRunning(false),
        sessionLost(false)
    {}

    void DoMainSessionStateChange(XrSessionState state);
    void DoOverlaySessionCommand(OverlayCommand command);
    void DoMainSessionCommand(OverlayCommand command);
    void DoMainSessionLostError()
    {
        sessionLossState = LOST;
    }
    
    typedef std::pair<bool, XrSessionState> OptionalSessionStateChange;
    bool isPendingStateChange;
    XrSessionState pendingStateChange;
    OptionalSessionStateChange DoPendingStateChange();
    OverlaySessionState GetState()
    {
        return state;
    }
    SessionLossState GetSessionLossState()
    {
        return sessionLossState;
    }
} overlayAppSession;

OptionalSessionStateChange DoPendingStateChange()
{
    if(isPendingStateChange) {
        OptionalSessionStateChange change {true, pendingStateChange};


        return change;
    } else {
        return OptionalSessionStateChange { false, XR_SESSION_STATE_UNKNOWN };
    }
}

void OverlayAppSession::DoMainSessionStateChange(XrSessionState newState)
{
    switch(overlayAppSession.state) {
        case IDLE:
            break;
        case READY:
            if(newState == XR_SESSION_STATE_READY) {
                if(!isPendingStateChange)
            }
            break;
        case SYNCHRONIZED:
            break;
        case VISIBLE:
            break;
        case FOCUSED:
            break;
        case STOPPING:
            break;
        case EXITING:
            break;
        case LOSS_PENDING:
            break;
        case STOPPING_SYNCHRONIZED:
            break;
        case STOPPING_VISIBLE:
            break;
        case STOPPING_FOCUSED:
            break;
    }
}

in all Session functions:
    if(overlayAppSession.GetSessionLossState() == LOST) {
        return XR_ERROR_SESSION_LOST;
    }
    if(overlayAppSession.GetSessionLossState() == LOSS_PENDING) {
        // simulate the operation
        return XR_SESSION_LOSS_PENDING;
    }

in Main xrPollEvent:
    if SessionStateChange
        overlayAppSession.DoMainSessionStateChange(stateChange)
    else
        queue

in Overlay xrPollEvent:
    std::pair<bool, XrSessionState> possibleStateChange;
    possibleStateChange = overlayAppSession.GetPendingStateChange;
    if(possibleStateChange.first) {
        copy to Overlay a synthetic new event with state change
    } else {
        pop and send event from end of queue
    }
