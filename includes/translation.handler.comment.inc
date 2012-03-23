<?php

/**
 * @file
 * Comment translation handler for the entity translation module.
 */


/**
 * Comment translation handler.
 */
class EntityTranslationCommentHandler extends EntityTranslationDefaultHandler {

  public function __construct($entity_type, $entity_info, $entity, $entity_id) {
    parent::__construct('comment', $entity_info, $entity, $entity_id);
  }

  public function getLanguage() {
    return $this->entity->language;
  }

  public function entityForm(&$form, &$form_state) {
    parent::entityForm($form, $form_state);
    // Adjust the translation fieldset weight to move it below the admin one.
    $form['translation']['#weight'] = 1;
  }

  protected function entityFormTitle() {
    return t('Edit comment @subject', array('@subject' => $this->getLabel()));
  }

  protected function getStatus() {
    return (boolean) $this->entity->status;
  }
}
