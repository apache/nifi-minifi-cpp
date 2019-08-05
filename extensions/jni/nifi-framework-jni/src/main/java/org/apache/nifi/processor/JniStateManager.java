package org.apache.nifi.processor;

import org.apache.nifi.components.state.Scope;
import org.apache.nifi.components.state.StateManager;
import org.apache.nifi.components.state.StateMap;

import java.util.Map;

public class JniStateManager  implements StateManager {



    @Override
    public void setState(final Map<String, String> state, final Scope scope) {
    }

    @Override
    public StateMap getState(final Scope scope) {
        return new JniStateMap(null,1);
    }

    @Override
    public boolean replace(final StateMap oldValue, final Map<String, String> newValue, final Scope scope) {
        return false;
    }

    @Override
    public void clear(final Scope scope) {
    }
}