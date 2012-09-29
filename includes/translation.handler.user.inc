<?php

/**
 * @file
 * User translation handler for the entity translation module.
 */


/**
 * User translation handler.
 */
class EntityTranslationUserHandler extends EntityTranslationDefaultHandler {

  public function __construct($entity_type, $entity_info, $entity, $entity_id) {
    parent::__construct('user', $entity_info, $entity, $entity_id);
  }

  /**
   * @see EntityTranslationDefaultHandler::getLanguageKey()
   */
  public function getLanguageKey() {
    return 'entity_language';
  }
}
