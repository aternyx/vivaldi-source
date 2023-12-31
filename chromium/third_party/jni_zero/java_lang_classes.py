# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# List generated via: unzip android.jar | grep java/lang | grep -v '$'
_NAMES = {
    'AbstractMethodError',
    'AbstractStringBuilder',
    'Appendable',
    'ArithmeticException',
    'ArrayIndexOutOfBoundsException',
    'ArrayStoreException',
    'AssertionError',
    'AutoCloseable',
    'Boolean',
    'Byte',
    'Character',
    'CharSequence',
    'ClassCastException',
    'ClassCircularityError',
    'Class',
    'ClassFormatError',
    'ClassLoader',
    'ClassNotFoundException',
    'Cloneable',
    'CloneNotSupportedException',
    'Comparable',
    'Compiler',
    'Deprecated',
    'Double',
    'Enum',
    'EnumConstantNotPresentException',
    'Error',
    'Exception',
    'ExceptionInInitializerError',
    'Float',
    'IllegalAccessError',
    'IllegalAccessException',
    'IllegalArgumentException',
    'IllegalMonitorStateException',
    'IllegalStateException',
    'IncompatibleClassChangeError',
    'IndexOutOfBoundsException',
    'InheritableThreadLocal',
    'InstantiationError',
    'InstantiationException',
    'Integer',
    'InternalError',
    'InterruptedException',
    'Iterable',
    'LinkageError',
    'Long',
    'Math',
    'NegativeArraySizeException',
    'NoClassDefFoundError',
    'NoSuchFieldError',
    'NoSuchFieldException',
    'NoSuchMethodError',
    'NoSuchMethodException',
    'NullPointerException',
    'Number',
    'NumberFormatException',
    'Object',
    'OutOfMemoryError',
    'Override',
    'Package',
    'ProcessBuilder',
    'Process',
    'Readable',
    'ReflectiveOperationException',
    'Runnable',
    'Runtime',
    'RuntimeException',
    'RuntimePermission',
    'SafeVarargs',
    'SecurityException',
    'SecurityManager',
    'Short',
    'StackOverflowError',
    'StackTraceElement',
    'StrictMath',
    'StringBuffer',
    'StringBuilder',
    'String',
    'StringIndexOutOfBoundsException',
    'SuppressWarnings',
    'System',
    'Thread',
    'ThreadDeath',
    'ThreadGroup',
    'ThreadLocal',
    'Throwable',
    'TypeNotPresentException',
    'UnknownError',
    'UnsatisfiedLinkError',
    'UnsupportedClassVersionError',
    'UnsupportedOperationException',
    'VerifyError',
    'VirtualMachineError',
    'Void',
}


def contains(unqualified_type_name):
  return unqualified_type_name in _NAMES
