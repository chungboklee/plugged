<?php

/** @generate-class-entries */

class PDOException extends RuntimeException
{
    /** @var int|string */
    protected $code = 0;
    public ?array $errorInfo = null;
}

/**
 * @return array<int, string>
 * @refcount 1
 */
function pdo_drivers(): array {}
