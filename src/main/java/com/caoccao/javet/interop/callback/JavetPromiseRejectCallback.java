/*
 * Copyright (c) 2021-2025. caoccao.com Sam Cao
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.caoccao.javet.interop.callback;

import com.caoccao.javet.enums.JavetPromiseRejectEvent;
import com.caoccao.javet.interfaces.IJavetLogger;
import com.caoccao.javet.values.V8Value;
import com.caoccao.javet.values.reference.V8ValuePromise;

/**
 * The type Javet promise reject callback.
 *
 * @since 0.8.3
 */
public class JavetPromiseRejectCallback implements IJavetPromiseRejectCallback {
    /**
     * The Logger.
     *
     * @since 0.8.3
     */
    protected IJavetLogger logger;

    /**
     * Instantiates a new Javet promise reject callback.
     *
     * @param logger the logger
     * @since 0.8.3
     */
    public JavetPromiseRejectCallback(IJavetLogger logger) {
        this.logger = logger;
    }

    @Override
    public void callback(JavetPromiseRejectEvent event, V8ValuePromise promise, V8Value value) {
        logger.logWarn("Received promise reject callback with event {0} {1}.", event.getCode(), event.getName());
    }
}
