/*
 * Copyright (c) 2024-2025. caoccao.com Sam Cao
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

package com.caoccao.javet.interop.binding;

import com.caoccao.javet.exceptions.JavetException;
import com.caoccao.javet.interop.V8Runtime;
import com.caoccao.javet.values.V8Value;

/**
 * The interface Class proxy plugin function.
 *
 * @param <E> the type parameter
 * @since 3.0.3
 */
public interface IClassProxyPluginFunction<E extends Exception> {
    /**
     * Invoke the function by the V8 runtime and the target object, then return a V8 value.
     *
     * @param v8Runtime    the V8 runtime
     * @param targetObject the target object
     * @return the V8 value
     * @throws JavetException the javet exception
     * @throws E              the custom exception
     * @since 3.0.3
     */
    V8Value invoke(V8Runtime v8Runtime, Object targetObject) throws JavetException, E;
}
