<?php

/**
 * @file
 * Comment translation handler for the entity translation module.
 */


/**
 * Comment translation handler.
 */
class EntityTranslationCommentHandler extends EntityTranslationDefaultHandler {

  public function __construct($entity_type, $entity_info, $entity) {
    parent::__construct('comment', $entity_info, $entity);
  }

  /**
   * @see EntityTranslationDefaultHandler::entityForm()
   */
  public function entityForm(&$form, &$form_state) {
    parent::entityForm($form, $form_state);
    // Adjust the translation fieldset weight to move it below the admin one.
    $form['translation']['#weight'] = 1;
  }

  /**
   * @see EntityTranslationDefaultHandler::entityFormTitle()
   */
  protected function entityFormTitle() {
    return t('Edit comment @subject', array('@subject' => $this->getLabel()));
  }

  /**
   * @see EntityTranslationDefaultHandler::getStatus()
   */
  protected function getStatus() {
    return (boolean) $this->entity->status;
  }

   /**
   * Overrides EntityTranslationController::getAccess().
   */
  public function getAccess($op) {
    switch ($op) {
      case 'view':
        return user_access('access comments');
      case 'update':
        return comment_access('edit', $this->entity);
      case 'delete':
        return user_access('administer comments');
      case 'create':
        return user_access('post comments');
    }
    return parent::getAccess($op);
  }

  /**
   * Overrides EntityTranslationController::getTranslationAccess().
   */
  public function getTranslationAccess($op) {
    return user_access('translate comments') && parent::getTranslationAccess($op);
  }
}
